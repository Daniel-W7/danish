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

#include "page.h"

#define SSH     "/usr/bin/ssh"
#define SHELL   "/bin/bash"

//添加gtk组件

static GtkWidget *m_notebook;
//static GtkWidget *m_menu;
//static GtkWidget *m_menu_copy;
//static GtkWidget *m_menu_paste;
//static GtkWidget *m_menu_copy_paste;

static int m_auto_focus = 1;
//定义pg->type == PG_TYPE_SSH的情况
static void *wait_ssh_child(void *p)
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
static void run_shell(pg_t *pg)
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
static void run_ssh(pg_t *pg)
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
    struct termios tio;//termios tty调用接口
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

        printf("\n");
        printf(PACKAGE" v"VERSION"\n");
        printf(COPYRIGHT"\n");
        printf("\n");
        printf("Connecting ... %s:%s\n", pg->ssh.cfg.host, pg->ssh.cfg.port);
        printf("\n");

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
//定义主进程，根据type的值打开shell和ssh
static void *work(void *p)
{
    pg_t *pg = (pg_t*) p;
    //type枚举
    switch (pg->type) {
    case PG_TYPE_SHELL:
        run_shell(pg); // 打开shell。block here;
        break;

    case PG_TYPE_SSH:  
        run_ssh(pg); // 打开ssh，block here;
        break;

    default:
        break;//直接退出
    }

    //gdk_threads_enter(); 命令已过期，暂时禁用
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(m_notebook), pg->body);//笔记本控件，能够让用户标签式地切换多个界面。
    gtk_notebook_remove_page(GTK_NOTEBOOK(m_notebook), num);
    //gdk_threads_leave();命令已过期，暂时禁用

    return NULL;
}
/*暂时禁用on_menu_copy_clicked
static void on_menu_copy_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
    //变量无用，暂时关闭
    VteTerminal *vte = (VteTerminal*) NULL;
    if (pg->type == PG_TYPE_SHELL) {
        vte = (VteTerminal*) pg->shell.vte;
    }
    else if (pg->type == PG_TYPE_SSH) {
        vte = (VteTerminal*) pg->ssh.vte;
    }

    vte_terminal_copy_clipboard(vte);//命令已过期，暂时禁用
    vte_terminal_select_none(vte);//命令已过期，暂时禁用
}
*/
/*
//定义快捷按钮命令的执行方法
static void on_cmd_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
    page_send_string_crlf(i, (char*) user_data);

    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
    gtk_widget_grab_focus(page);
}
//定义窗口快捷按钮
static void on_btn_clicked(GtkToolButton *item, gpointer user_data)
{
    GtkWidget *btn = (GtkWidget*) user_data;
    gtk_menu_popup_at_pointer(GTK_MENU(btn),NULL);
}
*/
/*
static void on_menu_copy_paste_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");

    VteTerminal *vte = (VteTerminal*) NULL;
    if (pg->type == PG_TYPE_SHELL) {
        vte = (VteTerminal*) pg->shell.vte;
    }
    else if (pg->type == PG_TYPE_SSH) {
        vte = (VteTerminal*) pg->ssh.vte;
    }

    //vte_terminal_copy_clipboard(vte);命令已过期，暂时禁用
    //vte_terminal_select_none(vte);命令已过期，暂时禁用
    vte_terminal_paste_clipboard(vte);
}
*/

// 标签选中改变时
// 1、修改标签颜色
// 2、使标签内vte获得焦点
static void on_notebook_switch(GtkNotebook *notebook, GtkWidget *page,
                               guint page_num, gpointer user_data)
{
    /*
    // 修改标签颜色(暂时先注释，暂时不用)
    //
    // 未被选中为黑色，被选中为红色
    //GdkColor color;
    //更新gdk_rgba_parse,暂时不设置标签颜色改变
    const gchar *color;

    pg_t *pg = NULL;
    gdk_rgba_parse("NULL", color);
    int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(m_notebook));
    int i = 0;
    for (i = 1; i<count; i++) {
        GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
        pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
        if (pg->head.label) {
            gtk_widget_override_color(pg->head.label, GTK_STATE_NORMAL, &color);     
        }
    }

    pg = (pg_t*) g_object_get_data(G_OBJECT(page), "pg");
    gdk_rgba_parse("NULL", color);
    if (pg->head.label) {
        gtk_widget_override_color(pg->head.label, GTK_STATE_NORMAL, &color);
    }
    */
    // 移动焦点到vte上
    if (m_auto_focus) {
        gtk_widget_grab_focus(page);
    }
}
//关闭对应page
static void on_close_clicked(GtkWidget *widget, gpointer user_data)
{
    pg_t *pg = (pg_t*) user_data;
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(m_notebook), pg->body);
    page_close(num);
}
/*
// 右键显示菜单
static gboolean on_vte_button_press(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    VteTerminal *vte = (VteTerminal*) widget;
    GdkEventButton *button = (GdkEventButton*) event;

    if (button->type == GDK_BUTTON_PRESS && // 按下
        button->button == 3) {  // 右键

        gtk_widget_set_sensitive(m_menu_copy, FALSE);
        gtk_widget_set_sensitive(m_menu_copy_paste, FALSE);
        gtk_widget_set_sensitive(m_menu_paste, FALSE);

        // copy / copy_paste
        if (vte_terminal_get_has_selection(vte)) {
            gtk_widget_set_sensitive(m_menu_copy, TRUE);
            gtk_widget_set_sensitive(m_menu_copy_paste, TRUE);
        }

        // paste
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (clipboard && gtk_clipboard_wait_is_text_available(clipboard)) {
            gtk_widget_set_sensitive(m_menu_paste, TRUE);
        }
        
        // popup menu
        gtk_menu_popup_at_pointer(GTK_MENU(m_menu),event);;
        //gtk_menu_popup_at_pointer(GTK_MENU(m_menu),NULL);;
    }

    return FALSE;
}
*/
int page_init(GtkWidget *hub_page) 
{
    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
	// popup menu
    //menu_create();//implicit declaration of function ‘menu_create’

    // notebook
    m_notebook = gtk_notebook_new();
    g_signal_connect_after(G_OBJECT(m_notebook), "switch-page", G_CALLBACK(on_notebook_switch), NULL);

/*    // 添加 hub 标签，暂时禁用
    // tab = hbox + label + button
    bzero(pg, sizeof(pg_t));
    pg->type = PG_TYPE_HUB;
    pg->head.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    pg->head.image = img_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);//warning: ‘GtkStock’ is deprecated 
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.image, FALSE, FALSE, 10);
    gtk_widget_show_all(pg->head.box);
*/
    // body
    pg->body = hub_page;
    g_object_set_data(G_OBJECT(pg->body), "pg", pg);

    // page,定义站点显示
    gint num = gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), pg->body, pg->head.box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(m_notebook), pg->body, TRUE);

    gtk_widget_show_all(m_notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(m_notebook), num);

    return 0;
}

int page_term()
{
    page_foreach_close();

    return 0;
}

gint page_shell_create()
{
    char *tmp;

    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
    bzero(pg, sizeof(pg_t));

    // tab = hbox + label + button
    pg->type = PG_TYPE_SHELL;
    pg->head.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    // pg->head.image = img_from_name(ICON_SHELL);
    // gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.image, FALSE, FALSE, 10);
    pg->head.label = gtk_label_new("Shell");
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.label, FALSE, FALSE, 10);
    pg->head.button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(pg->head.button), GTK_RELIEF_NONE);
    tmp = get_res_path(ICON_CLOSE);
    gtk_button_set_image(GTK_BUTTON(pg->head.button), gtk_image_new_from_file(tmp));
    free(tmp);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(pg->head.button), "clicked", G_CALLBACK(on_close_clicked), pg);
    gtk_widget_show_all(pg->head.box);

    // pty + vte
    pg->body = vte_terminal_new();
    pg->shell.vte = pg->body;

    //pg->shell.pty = vte_pty_new(VTE_PTY_DEFAULT, NULL); 未定义，暂时禁用
    //vte_terminal_set_pty_object((VteTerminal*)pg->shell.vte, pg->ssh.pty);//warning: implicit declaration of function ‘vte_terminal_set_pty_object’

    //vte_terminal_set_font_from_string((VteTerminal*)pg->shell.vte, "WenQuanYi Micro Hei Mono 11");//warning: implicit declaration of function ‘vte_terminal_set_font_from_string’
    vte_terminal_set_scrollback_lines((VteTerminal*)pg->shell.vte, 1024);
    vte_terminal_set_scroll_on_keystroke((VteTerminal*)pg->shell.vte, 1);
    g_object_set_data(G_OBJECT(pg->shell.vte), "pg", pg);
    //g_signal_connect(G_OBJECT(pg->shell.vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);

    // page
    gint num = gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), pg->body, pg->head.box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(m_notebook), pg->body, TRUE);

    gtk_widget_show_all(m_notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(m_notebook), num);

    pthread_t tid;
    pthread_create(&tid, NULL, work, pg);

    return num;
}

gint page_ssh_create(cfg_t *cfg)
{
    char *tmp;

    if (NULL == cfg) {
        return -1;
    }

    if (strlen(cfg->host) == 0 || strlen(cfg->port) == 0 ||
        strlen(cfg->user) == 0 || strlen(cfg->pass) == 0) {
        return -1;
    }

    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
    bzero(pg, sizeof(pg_t));

    // cfg
    memcpy(&pg->ssh.cfg, cfg, sizeof(cfg_t));

    // tab = hbox + label + button
    pg->type = PG_TYPE_SSH;
    pg->head.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    pg->head.image = img_from_name(ICON_SITE);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.image, FALSE, FALSE, 10);
    char title[256];
    sprintf(title, "%s", cfg->name);
    pg->head.label = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.label, FALSE, FALSE, 10);
    pg->head.button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(pg->head.button), GTK_RELIEF_NONE);
    tmp = get_res_path(ICON_CLOSE);
    gtk_button_set_image(GTK_BUTTON(pg->head.button), gtk_image_new_from_file(tmp));
    free(tmp);
    gtk_box_pack_start(GTK_BOX(pg->head.box), pg->head.button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(pg->head.button), "clicked", G_CALLBACK(on_close_clicked), pg);
    gtk_widget_show_all(pg->head.box);

    // body container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    pg->body = vbox;
    g_object_set_data(G_OBJECT(pg->body), "pg", pg);
/*
    // toolbar,暂时禁用
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
        GtkToolItem *item;
        int i = 0;
        for (i=0; i<BTN_MAX_COUNT && cfg->btn[i].name[0] != '\0'; i++) {
            //item = gtk_menu_tool_button_new(NULL, cfg->btn[i].name);
            item = gtk_tool_button_new(NULL, cfg->btn[i].name);
            gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

            GtkWidget *menu = gtk_menu_new();
            //g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(on_btn_clicked), menu);
            //gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(item), menu);

            int j = 0;
            for (j=0; j<CMD_MAX_COUNT && cfg->btn[i].cmd[j].name[0] != '\0'; j++) {
                GtkWidget *cmd = gtk_menu_item_new_with_label(cfg->btn[i].cmd[j].name);
                gtk_menu_attach(GTK_MENU(menu), cmd, 0, 1, j, j+1);
              //  g_signal_connect(G_OBJECT(cmd), "activate", G_CALLBACK(on_cmd_clicked), cfg->btn[i].cmd[j].str);
            }
            gtk_widget_show_all(menu);


            // |
            item = gtk_separator_tool_item_new();
            gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
        }
*/
    // pty + vte
    GtkWidget *vte = vte_terminal_new();
    pg->ssh.vte = vte;
    //vte_terminal_set_emulation((VteTerminal*) vte, "xterm");//warning: implicit declaration of function ‘vte_terminal_set_emulation’
    gtk_box_pack_start(GTK_BOX(vbox), vte, TRUE, TRUE, 0);
    pg->ssh.pty = vte_pty_new_sync(VTE_PTY_DEFAULT, NULL,NULL); //未定义，暂时禁用，此项会导致ssh窗口无法打开，无法进行远程连接,vte2.91需要将vte_pty_new改为vte_pty_new_sync
    vte_terminal_set_pty((VteTerminal*)vte, pg->ssh.pty);
    vte_terminal_set_font_scale((VteTerminal*)vte, 1.5);//定义pty终端缩放的大小
    vte_terminal_set_scrollback_lines((VteTerminal*)vte, 1024);
    vte_terminal_set_scroll_on_keystroke((VteTerminal*)vte, 1);
    //g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);

    // page
    gint num = gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), pg->body, pg->head.box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(m_notebook), pg->body, TRUE);

    gtk_widget_show_all(m_notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(m_notebook), num);

    pthread_t tid;
    pthread_create(&tid, NULL, work, pg);

    gtk_widget_grab_focus(vte);

    return num;
}

int page_foreach_close()
{
    int i;
    int count = page_get_count();
    for (i = 0; i<count; i++) {
        page_close(i);
    }

    return 0;
}

int page_foreach_send_char(char c)
{
    int i;

    int count = page_get_count();
    for (i = 0; i<count; i++) {
        GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
        pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
        if (pg->type == PG_TYPE_SSH) {
            int mine_master_fd = vte_pty_get_fd(pg->ssh.pty);
            write(mine_master_fd, &c, 1);
        }
    }

    return 0;
}

int page_foreach_send_string(char *str)
{
    int i;

    int count = page_get_count();
    for (i = 0; i<count; i++) {
        GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
        pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
        if (pg->type == PG_TYPE_SSH) {
            int mine_master_fd = vte_pty_get_fd(pg->ssh.pty);
            write(mine_master_fd, str, strlen(str));
        }
    }

    return 0;
}
/*
int page_send_string(int i, char *str)
{
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
    if (pg->type == PG_TYPE_SSH) {
        int mine_master_fd = vte_pty_get_fd(pg->ssh.pty);
        write(mine_master_fd, str, strlen(str));
    }

    return 0;
}
*/
/*
int page_send_string_crlf(int i, char *str)
{
    page_send_string(i, str);
    page_send_string(i, "\n");
    return 0;
}
*/
GtkWidget *page_get_notebook()
{
    return m_notebook;
}

int page_get_count()
{
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(m_notebook));
}

int page_get_select_num()
{
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
}

void page_set_select_num(int i)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(m_notebook), i);
}

void page_set_auto_focus(int b)
{
    m_auto_focus = (b!=0);
}
//定义关闭页面
int page_close(int n)
{
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(m_notebook), n);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
    if (pg->type == PG_TYPE_SSH) {
        kill(pg->ssh.child, SIGKILL);
    }
    if (pg->type == PG_TYPE_SHELL) {
        kill(pg->shell.child, SIGKILL);
    }

    return 0;
}
//定义关闭选中页面
int page_close_select()
{
    return page_close(page_get_select_num());
}

int page_set_title(int i, char *str)
{
    return -1;
}

