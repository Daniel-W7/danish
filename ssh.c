/*
 *
 * Sample showing how to makes SSH2 with X11 Forwarding works.
 *
 * Usage :
 * "ssh2 host user password [DEBUG]"
 */
#define _XOPEN_SOURCE
#define _GNU_SOURCE
/*定义页面显示内容，以及ssh连接，tty，pty配置内容*/
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>//里面包含pid_t等函数
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <stdio.h>
#include <ctype.h>

#include <libssh2.h>

#include "site.h"
#include "ssh.h"
#include "page.h"
//GtkWidget *notebook;

#define _PATH_UNIX_X "/tmp/.X11-unix/X%d"

/*
 * Chained list that contains channels and associated X11 socket for each X11
 * connections
 */
struct chan_X11_list {
    LIBSSH2_CHANNEL  *chan;
    int               sock;
    struct chan_X11_list *next;
};

struct chan_X11_list * gp_x11_chan = NULL;
struct termios         _saved_tio;

/*
 * Utility function to remove a Node of the chained list
 */
static void remove_node(struct chan_X11_list *elem)
{
    struct chan_X11_list *current_node = NULL;

    current_node = gp_x11_chan;

    if(gp_x11_chan == elem) {
        gp_x11_chan = gp_x11_chan->next;
        free(current_node);
        return;
    }

    while(current_node->next != NULL) {
        if(current_node->next == elem) {
            current_node->next = current_node->next->next;
            current_node = current_node->next;
            free(current_node);
            break;
        }
    }
}


static void session_shutdown(LIBSSH2_SESSION *session)
{
    libssh2_session_disconnect(session,
                                "Session Shutdown, Thank you for playing");
    libssh2_session_free(session);
}

static int _raw_mode(void)
{
    int rc;
    struct termios tio;

    rc = tcgetattr(fileno(stdin), &tio);
    if(rc != -1) {
        _saved_tio = tio;
        /* do the equivalent of cfmakeraw() manually, to build on Solaris */
        tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
        tio.c_oflag &= ~OPOST;
        tio.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
        tio.c_cflag &= ~(CSIZE|PARENB);
        tio.c_cflag |= CS8;
        rc = tcsetattr(fileno(stdin), TCSADRAIN, &tio);
    }
    return rc;
}

static int _normal_mode(void)
{
    int rc;
    rc = tcsetattr(fileno(stdin), TCSADRAIN, &_saved_tio);
    return rc;
}

/*
 * CallBack to initialize the forwarding.
 * Save the channel to loop on it, save the X11 forwarded socket to send
 * and receive info from our X server.
 */
static void x11_callback(LIBSSH2_SESSION *session, LIBSSH2_CHANNEL *channel,
                         char *shost, int sport, void **abstract)
{
    const char *display = NULL;
    char *ptr          = NULL;
    char *temp_buff    = NULL;
    int   display_port = 0;
    int   sock         = 0;
    int   rc           = 0;
    struct sockaddr_un addr;
    struct chan_X11_list *new;
    struct chan_X11_list *chan_iter;
    (void)session;
    (void)shost;
    (void)sport;
    (void)abstract;
    /*
     * Connect to the display
     * Inspired by x11_connect_display in openssh
     */
    display = getenv("DISPLAY");
    if(display != NULL) {
        if(strncmp(display, "unix:", 5) == 0 ||
            display[0] == ':') {
            /* Connect to the local unix domain */
            ptr = strrchr(display, ':');
            temp_buff = (char *) calloc(strlen(ptr + 1), sizeof(char));
            if(!temp_buff) {
                perror("calloc");
                return;
            }
            memcpy(temp_buff, ptr + 1, strlen(ptr + 1));
            display_port = atoi(temp_buff);
            free(temp_buff);

            sock = socket(AF_UNIX, SOCK_STREAM, 0);
            if(sock < 0)
                return;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof(addr.sun_path),
                     _PATH_UNIX_X, display_port);
            rc = connect(sock, (struct sockaddr *) &addr, sizeof(addr));

            if(rc != -1) {
                /* Connection Successfull */
                if(gp_x11_chan == NULL) {
                    /* Calloc ensure that gp_X11_chan is full of 0 */
                    gp_x11_chan = (struct chan_X11_list *)
                        calloc(1, sizeof(struct chan_X11_list));
                    gp_x11_chan->sock = sock;
                    gp_x11_chan->chan = channel;
                    gp_x11_chan->next = NULL;
                }
                else {
                    chan_iter = gp_x11_chan;
                    while(chan_iter->next != NULL)
                        chan_iter = chan_iter->next;
                    /* Create the new Node */
                    new = (struct chan_X11_list *)
                        malloc(sizeof(struct chan_X11_list));
                    new->sock = sock;
                    new->chan = channel;
                    new->next = NULL;
                    chan_iter->next = new;
                }
            }
            else
                close(sock);
        }
    }
    return;
}

/*
 * Send and receive Data for the X11 channel.
 * If the connection is closed, returns -1, 0 either.
 */
static int x11_send_receive(LIBSSH2_CHANNEL *channel, int sock)
{
    char *buf          = NULL;
    int   bufsize      = 8192;
    int   rc           = 0;
    int   nfds         = 1;
    LIBSSH2_POLLFD  *fds      = NULL;
    fd_set set;
    struct timeval timeval_out;
    timeval_out.tv_sec = 0;
    timeval_out.tv_usec = 0;


    FD_ZERO(&set);
    FD_SET(sock, &set);

    buf = calloc(bufsize, sizeof(char));
    if(!buf)
        return 0;

    fds = malloc(sizeof (LIBSSH2_POLLFD));
    if(!fds) {
        free(buf);
        return 0;
    }

    fds[0].type = LIBSSH2_POLLFD_CHANNEL;
    fds[0].fd.channel = channel;
    fds[0].events = LIBSSH2_POLLFD_POLLIN;
    fds[0].revents = LIBSSH2_POLLFD_POLLIN;

    rc = libssh2_poll(fds, nfds, 0);
    if(rc >0) {
        rc = libssh2_channel_read(channel, buf, bufsize);
        write(sock, buf, rc);
    }

    rc = select(sock + 1, &set, NULL, NULL, &timeval_out);
    if(rc > 0) {
        memset((void *)buf, 0, bufsize);

        /* Data in sock*/
        rc = read(sock, buf, bufsize);
        if(rc > 0) {
            libssh2_channel_write(channel, buf, rc);
        }
        else {
            free(buf);
            return -1;
        }
    }

    free(fds);
    free(buf);
    if(libssh2_channel_eof(channel) == 1) {
        return -1;
    }
    return 0;
}
//定义pg->type == PG_TYPE_SSH的情况
void *wait_ssh_child(void *p)
{
    //pg_t为page.h中定义的结构体
    pg_t *pg = (pg_t*) p;
   
        waitpid(pg->ssh.child, NULL, 0);
        pg->ssh.need_stop = 1;

    return NULL;
}
/*
 * Main, more than inspired by ssh2.c by Bagder
 */
int run_ssh(pg_t *pg)
{
    
    unsigned long hostaddr;
    int sock = 0;
    int rc = 0;
    struct sockaddr_in sin;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    int hostport;
    char *username = NULL;
    char *password = NULL;
    size_t bufsiz = 8193;
    char *buf = NULL;
    int set_debug_on = 0;
    int nfds = 1;
    LIBSSH2_POLLFD *fds = NULL;

    /* Chan List struct */
    struct chan_X11_list *current_node = NULL;

    /* Struct winsize for term size */
    struct winsize w_size;
    struct winsize w_size_bck;

    /* For select on stdin */
    fd_set set;
    struct timeval timeval_out;
    timeval_out.tv_sec = 0;
    timeval_out.tv_usec = 10;
    
    hostaddr = inet_addr(pg->ssh.cfg.host);
    hostport = atoi(pg->ssh.cfg.port);
    username = pg->ssh.cfg.user;
    password = pg->ssh.cfg.pass;
    
    //const char *host= cfg->host;
    //const char *host_p=cfg->host;
    //hostaddr = inet_addr("127.0.0.1");
    //hostport = 22 ;
    //username = "test";
    //password = "test";

    /* 
     *输出输入流程
     * [output flow-chart]
     * vte << vte_master_fd << vte_slave_fd << this_app << mine_master_fd << mine_slave_fd << ssh
     * 
     * [intput flow-chart]
     * vte >> vte_master_fd >> vte_slave_fd >> this_app >> mine_master_fd >> mine_slave_fd >> ssh
     */
    int vte_master_fd = vte_pty_get_fd(pg->ssh.pty);
    int vte_slave_fd = open(ptsname(vte_master_fd), O_RDWR);

    // open mine_master_fd
    int mine_master_fd = getpt();
    // grant and unlock slave
    char *slave = ptsname(mine_master_fd);//ptsname() -- 获得从伪终端名(slave pseudo-terminal)
    //将页面锁定到vte中
    if (grantpt(mine_master_fd) != 0 ||unlockpt(mine_master_fd) != 0) {
        return 0;
    }
    
    //定义了一个ssh的子进程，如果fork返回0，说明在ssh子进程中
    pg->ssh.child = fork();
    // child for exec
    if (pg->ssh.child == 0) {
        //setenv()用来改变或增加环境变量的内容。参数为环境变量名称字符串，变量内容，参数overwrite用来决定是否要改变已存在的环境变量。
        //setenv("TERM", "xterm", 1);
        // used by ssh open函数用来打开一个设备，他返回的是一个整型变量，如果这个值等于-1，说明打开文件出现错误，如果为大于0的值，那么这个值代表的就是文件描述符
        int mine_slave_fd = open(slave, O_RDWR);
        //重新创建一个session
        //setsid();

        //将pid进程的进程组ID设置成pgid，创建一个新进程组或加入一个已存在的进程组
        //setpgid(0, 0);
        //设备驱动程序中对设备的I/O通道进行管理的函数
        //ioctl(mine_slave_fd, TIOCGWINSZ, mine_slave_fd);
        //close ()关闭文件，关闭open()打开的文件
        //close(0);
        //close(1);
        //close(2);
        
        //用来复制参数oldfd 所指的文件描述词, 并将它拷贝至参数newfd 后一块返回
        dup2(mine_slave_fd, 0);
        dup2(mine_slave_fd, 1);
        dup2(mine_slave_fd, 2);

        printf(PACKAGE" v"VERSION"\n");
        printf(COPYRIGHT"\n");
        printf("Connecting ... %s:%s\n",pg->ssh.cfg.host,pg->ssh.cfg.port);
        //printf("\n");
        /*
        char host[512];
        memset(host, 0x00, sizeof(host));//在一段内存块中填充某个给定的值
        sprintf(host, "%s@%s", pg->ssh.cfg.user, pg->ssh.cfg.host);//将配置文件里的用户名和host，以类似root@127.0.0.1的方式输出到host中
        execlp(SSH, SSH, host, "-p", pg->ssh.cfg.port, NULL);//执行系统命令，进行ssh连接
        */

        rc = libssh2_init(0);
        if(rc != 0) {
            fprintf(stderr, "libssh2 initialization failed (%d)\n", rc);
            return 1;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock == -1) {
            perror("socket");
            return -1;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(hostport);
        sin.sin_addr.s_addr = hostaddr;

        rc = connect(sock, (struct sockaddr *) &sin,sizeof(struct sockaddr_in));
        if(rc != 0) {
            fprintf(stderr, "Failed to established connection!\n");
            return -1;
        }
        /* Open a session */
        session = libssh2_session_init();
        rc      = libssh2_session_handshake(session, sock);
        if(rc != 0) {
            fprintf(stderr, "Failed Start the SSH session\n");
            return -1;
        }

        if(set_debug_on == 1)
            libssh2_trace(session, LIBSSH2_TRACE_CONN);

            /* ignore pedantic warnings by gcc on the callback argument */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wpedantic"
            /* Set X11 Callback */
            libssh2_session_callback_set(session, LIBSSH2_CALLBACK_X11,
                                        (void *)x11_callback);
        #pragma GCC diagnostic pop

        /* Authenticate via password */
        rc = libssh2_userauth_password(session, username, password);
        if(rc != 0) {
            fprintf(stderr, "Failed to authenticate\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }

        /* Open a channel */
        channel  = libssh2_channel_open_session(session);
        if(channel == NULL) {
            fprintf(stderr, "Failed to open a new channel\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }
        /* Request a PTY */
        //char *slave = ptsname(pg->body.pty);
        //rc = libssh2_channel_request_pty(channel,slave);
        //char *pty = pg->ssh.pty;
        rc = libssh2_channel_request_pty(channel,"xterm");
        if(rc != 0) {
            fprintf(stderr, "Failed to request a pty\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }

        /* Request X11 */
        rc = libssh2_channel_x11_req(channel, 0);
        if(rc != 0) {
            fprintf(stderr, "Failed to request X11 forwarding\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }

        /* Request a shell */
        rc = libssh2_channel_shell(channel);
        if(rc != 0) {
            fprintf(stderr, "Failed to open a shell\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }

        rc = _raw_mode();
        if(rc != 0) {
            fprintf(stderr, "Failed to entered in raw mode\n");
            session_shutdown(session);
            close(sock);
            return -1;
        }

        memset(&w_size, 0, sizeof(struct winsize));
        memset(&w_size_bck, 0, sizeof(struct winsize));

        while(1) {

            //将指定的文件描述符集清空
            FD_ZERO(&set);
            //在文件描述符集合中增加一个新的文件描述符
            FD_SET(fileno(stdin), &set);

            /* Search if a resize pty has to be send */
            //fileno()用来取得参数stream 指定的文件流所使用的文件描述词.
            ioctl(fileno(stdin), TIOCGWINSZ, &w_size);
            if((w_size.ws_row != w_size_bck.ws_row) ||(w_size.ws_col != w_size_bck.ws_col)) 
                {
                    w_size_bck = w_size;
                    libssh2_channel_request_pty_size(channel,w_size.ws_col,w_size.ws_row);
                }

            buf = calloc(bufsiz, sizeof(char));
            if(buf == NULL)
                break;

            fds = malloc(sizeof (LIBSSH2_POLLFD));
            if(fds == NULL) {
                free(buf);
                break;
            }

            fds[0].type = LIBSSH2_POLLFD_CHANNEL;
            fds[0].fd.channel = channel;
            fds[0].events = LIBSSH2_POLLFD_POLLIN;
            fds[0].revents = LIBSSH2_POLLFD_POLLIN;

            rc = libssh2_poll(fds, nfds, 0);
            if(rc >0) {
                libssh2_channel_read(channel, buf, sizeof(buf));
                fprintf(stdout, "%s", buf);
                fflush(stdout);
            }

            /* Looping on X clients */
            if(gp_x11_chan != NULL) {
                current_node = gp_x11_chan;
            }
            else
                current_node = NULL;

            while(current_node != NULL) {
                struct chan_X11_list *next_node;
                rc = x11_send_receive(current_node->chan, current_node->sock);
                next_node = current_node->next;
                if(rc == -1) {
                    shutdown(current_node->sock, SHUT_RDWR);
                    close(current_node->sock);
                    remove_node(current_node);
                }

                current_node = next_node;
            }


            rc = select(fileno(stdin) + 1, &set, NULL, NULL, &timeval_out);
            if(rc > 0) {
                /* Data in stdin*/
                rc = read(fileno(stdin), buf, 1);
                if(rc > 0)
                    libssh2_channel_write(channel, buf, sizeof(buf));
            }

            free(fds);
            free(buf);

            if(libssh2_channel_eof (channel) == 1) {
                break;
            }
        }

        if(channel) {
            libssh2_channel_free(channel);
            channel = NULL;
        }
        
        _normal_mode();

    }

    // thread for waitpid，线程管理，管理单个notebook页面的开启关闭操作
    pthread_t tid;//声明线程ID
    pthread_create(&tid, NULL, wait_ssh_child, pg);//创建一个线程
    
    // for expect
    int already_login = 0;
    struct winsize old_size = {0,0,0,0};
    int row = 0;
    int col = 0;
    //fd_set set;
    struct timeval tv = {0, 100};
    //若是ssh没有关闭，则执行下面的循环
    while (pg->ssh.need_stop == 0) {
        // vte_pty的尺寸变化时，修改mine_master_fd,以便它去通知ssh
        if (vte_pty_get_size(pg->ssh.pty, &row, &col, NULL) == TRUE) {
            if (row != old_size.ws_row ||col != old_size.ws_col) 
            {
                old_size.ws_row = (short) row;
                old_size.ws_col = (short) col;
                ioctl(mine_master_fd, TIOCSWINSZ, &old_size);
            }
        }

        //
        // 打印ssh返回的输出，并判断是否应该执行自动输入
        //
        //vte >> vte_master_fd >> vte_slave_fd >>
        // this_app >> mine_master_fd >> mine_slave_fd >> ssh
        FD_ZERO(&set);//将指定的文件描述符集清空
        FD_SET(mine_master_fd, &set);//用于在文件描述符集合中增加一个新的文件描述符。
        if (select(mine_master_fd+1, &set, NULL, NULL, &tv) > 0) 
        {
            char buf[256];
            int len = read(mine_master_fd, buf, sizeof(buf));//读文件函数(由已打开的文件读取数据)
            if (len > 0) {
                //write()会把参数buf 所指的内存写入count 个字节到参数fd 所指的文件内
                write(vte_slave_fd, buf, len);

                // 如果当前没有登录成功, 自动输入密码
                if (already_login == 0 && str_is_endwith(buf, len, SSH_PASSWORD)) {
                    already_login = 1;
                    write(mine_master_fd, pg->ssh.cfg.pass, strlen(pg->ssh.cfg.pass));
                    write(mine_master_fd, "\n", 1);
                }
            }
        }

        //
        // 读取vte_pty的用户输入，并发送到给ssh
        //vte >> vte_master_fd >> vte_slave_fd >> this_app >> mine_master_fd >> mine_slave_fd >> ssh
        // vte_slave_fd -> mine_master_fd
        //将指定的文件描述符集清空
        FD_ZERO(&set);
        //用于在文件描述符集合中增加一个新的文件描述符。
        FD_SET(vte_slave_fd, &set);
        if (select(vte_slave_fd+1, &set, NULL, NULL, &tv) > 0) {
            char buf[256];
            //读文件函数(由已打开的文件读取数据)
            int len = read(vte_slave_fd, buf, sizeof(buf));
            if (len > 0) {
                //write()会把参数buf 所指的内存写入count 个字节到参数fd 所指的文件内
                write(mine_master_fd, buf, len);
            }
        }

        usleep(1000);//usleep功能把进程挂起一段时间， 单位是微秒（百万分之一秒）；
    }

    //vte_pty_close(pg->ssh.pty);//命令已过期，暂时禁用

    libssh2_exit();

    return 0;
}
