/*
 *
 * Sample showing how to makes SSH2 with X11 Forwarding works.
 *
 * Usage :
 * "ssh2 host user password [DEBUG]"
 */

#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>

#include <libssh2.h>

#include "site.h"
#include "ssh.h"

#define _PATH_UNIX_X "/tmp/.X11-unix/X%d"

//GtkWidget *notebook;

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

/*
 * Main, more than inspired by ssh2.c by Bagder
 */
int run_ssh(cfg_t *cfg)
//int run_ssh(cfg_t *cfg)
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
/*
    hostaddr = inet_addr(cfg->host);
    hostport =	cfg->port;
    username = cfg->user;
    password = cfg->pass;
  */
//const char *host= cfg->host;
    hostaddr = inet_addr(cfg->host);
    hostport = 22 ;
    username = "test";
    password = "test";

/*
    if(argc > 3) {
        hostaddr = inet_addr(argv[1]);

        username = argv[2];
        password = argv[3];
    }
    else {
        fprintf(stderr, "Usage: %s destination username password",
                argv[0]);
        return -1;
    }

    if(argc > 4) {
        set_debug_on = 1;
        fprintf(stderr, "DEBUG is ON: %d\n", set_debug_on);
    }
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

    rc = connect(sock, (struct sockaddr *) &sin,
                 sizeof(struct sockaddr_in));
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
    rc = libssh2_channel_request_pty(channel, "xterm");
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

        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set);

        /* Search if a resize pty has to be send */
        ioctl(fileno(stdin), TIOCGWINSZ, &w_size);
        if((w_size.ws_row != w_size_bck.ws_row) ||
           (w_size.ws_col != w_size_bck.ws_col)) {
            w_size_bck = w_size;

            libssh2_channel_request_pty_size(channel,
                                             w_size.ws_col,
                                             w_size.ws_row);
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

    libssh2_exit();

    return 0;
}
static void *work(void *p)
{   
    
    cfg_t *cfg= (cfg_t*) p;
    run_ssh(cfg);
    //gdk_threads_enter();
  //  pg_t *pg = (pg_t*) p;
 //   int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pg->body);
 //   gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), num);
    //gdk_threads_leave();
    
    return NULL;
}
//创建新的ssh页面
gint page_ssh_create(cfg_t *cfg)
{
    char *tmp;

    if (NULL == cfg) {
        return -1;
    }
    if (cfg->host == NULL || cfg->port == 0 ||
        strlen(cfg->user) == 0 || strlen(cfg->pass) == 0) {
        return -1;
    }
    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
    bzero(pg, sizeof(pg_t));

    // cfg,配置文件
    memcpy(&pg->ssh.cfg, cfg, sizeof(cfg_t));

    // tab = hbox + label + button,定义新的窗口的头部信息
    pg->head.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    //gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.image, FALSE, FALSE, 10);
    char title[256];
    sprintf(title, "%s", cfg->name);
    pg->head.label = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.label, FALSE, FALSE, 10);
    //定义关闭按钮
    pg->head.button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(pg->head.button), GTK_RELIEF_NONE);
    tmp = get_res_path(ICON_CLOSE);
    gtk_button_set_image(GTK_BUTTON(pg->head.button), gtk_image_new_from_file(tmp));
    free(tmp);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.button, FALSE, FALSE, 0);
    //g_signal_connect(G_OBJECT(pg->head.button), "clicked", G_CALLBACK(on_close_clicked), pg);
    gtk_widget_show_all(pg->head.box);

    // body container,用于打开ssh界面
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    pg->body = vbox;
    g_object_set_data(G_OBJECT(pg->body), "pg", pg);
    // pty + vte
    GtkWidget *vte = vte_terminal_new();
    pg->ssh.vte = vte;
    
    //vte_terminal_set_emulation((VteTerminal*) vte, "xterm");//warning: implicit declaration of function ‘vte_terminal_set_emulation’
    gtk_box_pack_start(GTK_BOX(vbox), vte, TRUE, TRUE, 0);
    //未定义，暂时禁用，此项会导致ssh窗口无法打开，无法进行远程连接,vte2.91需要将vte_pty_new改为vte_pty_new_sync
    //pg->ssh.pty = vte_pty_new_sync(VTE_PTY_DEFAULT, NULL,NULL); 
    //vte_terminal_set_pty((VteTerminal*)vte, pg->ssh.pty);
    //定义pty终端缩放的大小
    //vte_terminal_set_font_scale((VteTerminal*)vte, 1.5);
    //vte_terminal_set_scrollback_lines((VteTerminal*)vte, 1024);
    //vte_terminal_set_scroll_on_keystroke((VteTerminal*)vte, 1);
    //g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);
    // page
    //gint num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pg->body, pg->head.box);
    //gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), pg->body, TRUE);

    //gtk_widget_show_all(notebook);
    //gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), num);

    pthread_t tid;
    pthread_create(&tid, NULL, work, pg);

    //gtk_widget_grab_focus(vte);

   return 0;
}
