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

#include "util.h"

#include "shell.h"

#define SSH     "/usr/bin/ssh"
#define SHELL   "/bin/bash"
//定义pg->type == PG_TYPE_SSH的情况
void *wait_ssh_child(void *p)
{
    //pg_t为page.h中定义的结构体
    pg_t *pg = (pg_t*) p;
    //type为pt_g内部定义的枚举变量，包含PG_TYPE_HUB,PG_TYPE_SSH,PG_TYPE_SHELL
    if (pg->type == PG_TYPE_SSH) {
        waitpid(pg->ssh.child, NULL, 0);
        pg->ssh.need_stop = 1;
    }

    return NULL;
}
//打开shell
void run_shell(pg_t *pg)
{
    //如果pg为空或者type不等于PG_TYPE_SHELL，返回空
    if (NULL == pg || pg->type != PG_TYPE_SHELL) {
        return;
    }

    pg->shell.child = fork();//定义了一个shell的子进程，如果fork返回0，说明在shell子进程中
    if (pg->shell.child == 0) {
        vte_pty_child_setup(pg->shell.pty);
        execlp(SHELL, SHELL, NULL);//excute shell
    }
    waitpid(pg->shell.child, NULL, 0);//等待子进程消失

    //vte_pty_close(pg->shell.pty); //关闭vtepty终端
}
//打开并建立ssh连接
void run_ssh(pg_t *pg)
{
    //如果pg为空或者type不等于PG_TYPE_SSH，返回空
    if (NULL == pg || pg->type != PG_TYPE_SSH) {
        return;
    }

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

    // raw 模式
    struct termios tio;//termios tty调用接口// 打开ssh，block here;
    tcgetattr(vte_slave_fd, &tio);//tcgetattr函数用于获取与终端相关的参数。参数fd为终端的文件描述符，返回的结果保存在termios结构体中
    cfmakeraw(&tio);//初始化 termios结构体,将他的一些成员初始化为默认的设置
    tcsetattr(vte_slave_fd, TCSADRAIN, &tio);//tcsetattr函数用于设置终端的相关参数。参数fd为打开的终端文件描述符，参数optional_actions用于控制修改起作用的时间，而结构体termios_p中保存了要修改的参数。

    // open mine_master_fd
    int mine_master_fd = getpt();
    if (mine_master_fd < 0) {
        return;
    }
    int flags = fcntl(mine_master_fd, F_GETFL, 0);//F_GETFL 取得文件描述词状态旗标, 此旗标为open()的参数flags.，返回值：成功则返回0, 若有错误则返回-1, 错误原因存于errno.
    if (flags < 0) {
        return;
    }
    flags &= ~O_NONBLOCK; // blocking it，阻塞进程
    flags |= FD_CLOEXEC; // close on exec
    if (fcntl(mine_master_fd, F_SETFL, flags) < 0) {
        return;
    }

    // grant and unlock slave
    char *slave = ptsname(mine_master_fd);//ptsname() -- 获得从伪终端名(slave pseudo-terminal)
    if (grantpt(mine_master_fd) != 0 ||unlockpt(mine_master_fd) != 0) {
        return;
    }

    pg->ssh.child = fork();//定义了一个ssh的子进程，如果fork返回0，说明在ssh子进程中
    // child for exec
    if (pg->ssh.child == 0) {
        setenv("TERM", "xterm", 1);//setenv()用来改变或增加环境变量的内容。参数为环境变量名称字符串，变量内容，参数overwrite用来决定是否要改变已存在的环境变量。
        int mine_slave_fd = open(slave, O_RDWR);   // used by sshopen函数用来打开一个设备，他返回的是一个整型变量，如果这个值等于-1，说明打开文件出现错误，如果为大于0的值，那么这个值代表的就是文件描述符
        setsid();//重新创建一个session
        setpgid(0, 0);//将pid进程的进程组ID设置成pgid，创建一个新进程组或加入一个已存在的进程组
        ioctl(mine_slave_fd, TIOCSCTTY, mine_slave_fd);//设备驱动程序中对设备的I/O通道进行管理的函数

        close(0);//close ()关闭文件，挂壁open()打开的文件
        close(1);
        close(2);
        dup2(mine_slave_fd, 0);//用来复制参数oldfd 所指的文件描述词, 并将它拷贝至参数newfd 后一块返回
        dup2(mine_slave_fd, 1);
        dup2(mine_slave_fd, 2);

        //printf("\n");
        printf(PACKAGE" v"VERSION"\n");
        printf(COPYRIGHT"\n");
        printf("\n");
        printf("Connecting ... %s:%s\n", pg->ssh.cfg.host, pg->ssh.cfg.port);
        //printf("\n");

        char host[512];
        memset(host, 0x00, sizeof(host));//在一段内存块中填充某个给定的值
        sprintf(host, "%s@%s", pg->ssh.cfg.user, pg->ssh.cfg.host);//将配置文件里的用户名和host，以类似root@127.0.0.1的方式输出到host中
        execlp(SSH, SSH, host, "-p", pg->ssh.cfg.port, NULL);//执行系统命令，进行ssh连接
    }

    // thread for waitpid
    pthread_t tid;//声明线程ID
    pthread_create(&tid, NULL, wait_ssh_child, pg);//创建一个线程
    
    // for expect
    int already_login = 0;
    struct winsize old_size = {0,0,0,0};
    int row = 0;
    int col = 0;
    fd_set set;
    struct timeval tv = {0, 100};
    //若是ssh没有关闭，则执行下面的循环
    while (pg->ssh.need_stop == 0) {
        // vte_pty的尺寸变化时，修改mine_master_fd,以便它去通知ssh
        if (vte_pty_get_size(pg->ssh.pty, &row, &col, NULL) == TRUE) {
            if (row != old_size.ws_row ||
                col != old_size.ws_col) {
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
        if (select(mine_master_fd+1, &set, NULL, NULL, &tv) > 0) {
            char buf[256];
            int len = read(mine_master_fd, buf, sizeof(buf));//读文件函数(由已打开的文件读取数据)
            if (len > 0) {
                write(vte_slave_fd, buf, len);//write()会把参数buf 所指的内存写入count 个字节到参数fd 所指的文件内

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
        FD_ZERO(&set);//将指定的文件描述符集清空
        FD_SET(vte_slave_fd, &set);//用于在文件描述符集合中增加一个新的文件描述符。
        if (select(vte_slave_fd+1, &set, NULL, NULL, &tv) > 0) {
            char buf[256];
            int len = read(vte_slave_fd, buf, sizeof(buf));//读文件函数(由已打开的文件读取数据)
            if (len > 0) {
                write(mine_master_fd, buf, len);//write()会把参数buf 所指的内存写入count 个字节到参数fd 所指的文件内
            }
        }

        usleep(1000);//usleep功能把进程挂起一段时间， 单位是微秒（百万分之一秒）；
    }

    //vte_pty_close(pg->ssh.pty);//命令已过期，暂时禁用
}
