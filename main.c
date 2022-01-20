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

#include "page.h"
#include "site.h"
//gtk初始化组件
GtkWidget *m_window;
GtkWidget *hbox;//横向窗口
GtkWidget *vbox;//纵向窗口
GtkWidget *sidebar;
GtkWidget *stack;
GtkWidget *widget;
GtkWidget *notebook;
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
static int window_create_show()
{
    //char *tmp;

    // window,初始化定义窗口
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "danish");//设置窗口名称
	//gtk_window_maximize(GTK_WINDOW(m_window));//设置全屏显示,注释掉似乎也不影响显示
	gtk_window_set_position(GTK_WINDOW(m_window),GTK_WIN_POS_NONE);//设置窗口在显示器中的位置为居中
		/*
		   	GTK_WIN_POS_NONE： 不固定
			GTK_WIN_POS_CENTER: 居中
			GTK_WIN_POS_MOUSE: 出现在鼠标位置
			GTK_WIN_POS_CENTER_ALWAYS: 窗口总是居中
		 */
	gtk_widget_set_size_request(m_window,970,600);//设置窗口的初始大小，黄金比例1：1.618
    
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);//横向显示窗口,显示侧边栏
		//横向第一个窗口,sidebar
		//定义侧边栏
			//sidebar = page_get_sidebar();
			sidebar = gtk_stack_sidebar_new();
			//gtk_window_set_default_size(sidebar,50,600);
			gtk_widget_set_size_request(sidebar,150,600);
			gtk_box_pack_start(GTK_BOX(hbox), sidebar, TRUE, TRUE, 0);

		//横向第二个窗口 notebook
			notebook = page_get_notebook();
			//gtk_window_set_default_size(notebook,750,600);
			gtk_widget_set_size_request(notebook,750,600);
			gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 1);

	gtk_container_add(GTK_CONTAINER(m_window), hbox);
	
	gtk_widget_set_events(m_window, GDK_BUTTON_PRESS_MASK|GDK_KEY_PRESS_MASK);
    	g_signal_connect(G_OBJECT(m_window), "key-press-event", G_CALLBACK(on_window_key_press), NULL);
	g_signal_connect(G_OBJECT(m_window), "destroy", G_CALLBACK (gtk_main_quit), NULL);//定义点击关闭退出

	gtk_widget_show_all(m_window);

    return 0;
}

const char *HOME = NULL;
char PATH[256] = {0x00};
//初始化，找到家目录，并创建对应目录
int init()
{
    // home
    HOME = getenv("HOME");
    if (HOME == NULL) {
        return -1;
    }

    // path
    memset(PATH, 0x00, sizeof(PATH));
    sprintf(PATH, "%s/%s", HOME, CONFIG_DIR);

    // mkdir PATH if it is not exits.
    char cmd[512];
    memset(cmd, 0x00, sizeof(cmd));
    sprintf(cmd, "mkdir -p %s", PATH);
    system(cmd);

    return 0;
}
//主程序
int main(int argc, char **argv)
{
    if (init() != 0) {
        return -1;
    }

    // 初始化
    gtk_init(&argc, &argv);

    // 创建site页
    site_init();

    // 装载site配置
    site_load();

    // 创建tab容器（使用site页作为hub_page)
    //读取site的配置信息
	GtkWidget *hub = site_get_object();
	//初始化页面
    page_init(hub);
    gtk_widget_grab_focus(hub);

    // 创建主窗口
    window_create_show();

    gtk_main();

    // 回收资源
    site_term();

    return 0;
}
