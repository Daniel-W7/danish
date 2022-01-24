#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <signal.h>
#include <fcntl.h>

#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "config.h"
#include "util.h"

#include "shell.h"
#include "page.h"
#include "site.h"
//gtk初始化组件
//添加gtk组件

GtkWidget *window;
GtkWidget *hbox;//横向窗口
GtkWidget *vbox;//纵向窗口
GtkWidget *sidebar;
GtkWidget *stack;
GtkWidget *widget;
GtkWidget *notebook;
static int m_auto_focus = 1;
int page_get_count()
{
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
}

int page_get_select_num()
{
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
}

void page_set_select_num(int i)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), i);
}

void page_set_auto_focus(int b)
{
    m_auto_focus = (b!=0);
}
//定义关闭页面
int page_close(int n)
{
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), n);
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
// 标签选中切换时
// 1、修改标签颜色,暂时删除
// 2、使标签内vte获得焦点
static void on_notebook_switch(GtkNotebook *notebook, GtkWidget *page,
                               guint page_num, gpointer user_data)
{
    // 移动焦点到vte上
    if (m_auto_focus) {
        gtk_widget_grab_focus(page);
    }
}
//关闭对应page
static void on_close_clicked(GtkWidget *widget, gpointer user_data)
{
    pg_t *pg = (pg_t*) user_data;
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pg->body);
    page_close(num);
}
//定义主进程，根据type的值打开shell和ssh
static void *work(void *p)
{
    pg_t *pg = (pg_t*) p;
    //type枚举
    switch (pg->type) {
    case PG_TYPE_SHELL:
        // 打开shell。block here;
        run_shell(pg); 
        break;

    case PG_TYPE_SSH:  
        // 打开ssh，block here;
        run_ssh(pg); 
        break;

    default:
        break;//直接退出
    }
	//推出后关闭所有的索引和notebook的显示
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pg->body);//笔记本控件，能够让用户标签式地切换多个界面。
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), num);

    return NULL;
}
//创建本地shell页面
gint page_shell_create()
{
    char *tmp;

    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
    bzero(pg, sizeof(pg_t));

    // tab = hbox + label + button
    pg->type = PG_TYPE_SHELL;
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
    gint num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pg->body, pg->head.box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), pg->body, TRUE);

    gtk_widget_show_all(notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), num);

    pthread_t tid;
    pthread_create(&tid, NULL, work, pg);

    return num;
}
//创建新的ssh页面
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

    // body container,用于打开ssh界面
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    pg->body = vbox;
    g_object_set_data(G_OBJECT(pg->body), "pg", pg);
    // pty + vte
    GtkWidget *vte = vte_terminal_new();
    pg->ssh.vte = vte;
    //vte_terminal_set_emulation((VteTerminal*) vte, "xterm");//warning: implicit declaration of function ‘vte_terminal_set_emulation’
    gtk_box_pack_start(GTK_BOX(vbox), vte, TRUE, TRUE, 0);
    pg->ssh.pty = vte_pty_new_sync(VTE_PTY_DEFAULT, NULL,NULL); //未定义，暂时禁用，此项会导致ssh窗口无法打开，无法进行远程连接,vte2.91需要将vte_pty_new改为vte_pty_new_sync
    vte_terminal_set_pty((VteTerminal*)vte, pg->ssh.pty);
    vte_terminal_set_font_scale((VteTerminal*)vte, 1.5);//定义pty终端缩放的大小
    vte_terminal_set_scrollback_lines((VteTerminal*)vte, 1024);
    //vte_terminal_set_scroll_on_keystroke((VteTerminal*)vte, 1);
    //g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);

    // page
    gint num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pg->body, pg->head.box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), pg->body, TRUE);

    gtk_widget_show_all(notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), num);

    pthread_t tid;
    pthread_create(&tid, NULL, work, pg);

    gtk_widget_grab_focus(vte);

    return num;
}
//定义窗口打开关闭移动的操作
static gboolean on_window_key_press(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    GdkEventKey *key = (GdkEventKey*) event;

    if ((key->state & GDK_CONTROL_MASK) &&
        (key->state & GDK_SHIFT_MASK)) {

        // 关闭当前窗口
        if (key->keyval == GDK_KEY_W) {
            page_close_select();
            return TRUE;
        }

        // 打开一个本地窗口
        if (key->keyval == GDK_KEY_T) {
            page_shell_create();
            return TRUE;
        }
    }
    return FALSE;
}
//创建窗口
int window_create(GtkWidget *hub_page)
{

    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));

    //定义notebook显示的label
        pg->head.label = gtk_label_new("Sessions");

     // body,定义页面主要内容
	    pg->body = hub_page;

    //将body里面的树状站点信息放置到label中
	    //const char *label = pg->body;

	// window,初始化定义窗口
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//设置窗口名称
	gtk_window_set_title(GTK_WINDOW(window), "danish");
	//设置窗口在显示器中的位置为任意
	gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_NONE);
		/*
		   	GTK_WIN_POS_NONE： 不固定
			GTK_WIN_POS_CENTER: 居中
			GTK_WIN_POS_MOUSE: 出现在鼠标位置
			GTK_WIN_POS_CENTER_ALWAYS: 窗口总是居中
		 */
	//设置窗口的初始大小，黄金比例1：1.618
	gtk_widget_set_size_request(window,970,600);
    
	//创建窗口容器vbox，用来显示配置信息,配置为VERTICAL，纵向显示组件
	//vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	//横向显示窗口,显示侧边栏
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
   		/* 
		//测试窗口
		widget = gtk_image_new_from_icon_name("face-angry", GTK_ICON_SIZE_MENU);
        	gtk_image_set_pixel_size(GTK_IMAGE(widget), 100);
		gtk_container_add(GTK_CONTAINER(hbox), widget);
		*/
	    // notebook,创建notebook
	    notebook = gtk_notebook_new();
	    //定义切换notebook页面的操作
	    g_signal_connect_after(G_OBJECT(notebook), "switch-page", G_CALLBACK(on_notebook_switch), NULL);
	    
	    //将body里面的内容连接到Sessions label下面
	    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pg->body, pg->head.label);
	/*
        // page,定义sidebar,显示内容
        sidebar = gtk_stack_sidebar_new();
        //定义stack,栈，用于定义sidebar的内容
	    stack = gtk_stack_new(); 
        //将sidebar和stack连接起来
	    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));
        
        //将notebook加入到stack中，并与label对应
	    gtk_stack_add_named(GTK_STACK(stack), notebook, "label");
	    */
        gtk_container_add(GTK_CONTAINER(hbox), notebook);
 
	gtk_container_add(GTK_CONTAINER(window), hbox);
    	//gtk_container_add(GTK_CONTAINER(window), notebook);
	//gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_set_events(window, GDK_BUTTON_PRESS_MASK|GDK_KEY_PRESS_MASK);
	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_window_key_press), NULL);
    	//定义退出按钮
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all(window);
	//gtk_main();
	return 0;
}
