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
    
	//创建窗口容器vbox，用来显示配置信息,配置为VERTICAL，纵向显示组件
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);//横向显示窗口,显示侧边栏
	/*		//横向第一个窗口
		    sidebar = gtk_stack_sidebar_new();//定义侧边栏
			gtk_box_pack_start(GTK_BOX(hbox), sidebar, TRUE, TRUE, 0);//侧边栏hbox里面显示sidebar
			stack = gtk_stack_new();//定义stack,栈，用于定义sidebar的内容
			gtk_stack_set_transition_type(GTK_STACK(stack),GTK_STACK_TRANSITION_TYPE_SLIDE_UP);//站点的切换的效果是往上的
			gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));//将sidebar和stack连接起来
			gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);//侧边栏hbox里面显示stack的内容
	*/
	/*
			//横向第二个窗口
			//第一个图标
			widget = gtk_image_new_from_icon_name("face-angry", GTK_ICON_SIZE_MENU);//定义图标widget
			gtk_image_set_pixel_size(GTK_IMAGE(widget), 150);//定义图标大小
			gtk_stack_add_named(GTK_STACK(stack), widget, "angry");//将图标加入到stack中，并与字符angry对应
			gtk_container_child_set(GTK_CONTAINER(stack), widget, "title", "angry", NULL);//配置容器中的图标和字符对应策略
			//第二个图标
			gtk_stack_add_named(GTK_STACK(stack), notebook , "sick");
			gtk_container_child_set(GTK_CONTAINER(stack), notebook , "title","sick", NULL);
  */
			// notebook
			notebook = page_get_notebook();
//			gtk_stack_add_named(GTK_STACK(stack), notebook , "sick");
//			gtk_container_child_set(GTK_CONTAINER(stack), notebook , "title","sick", NULL);

			gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 1);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);//显示vbox和hbox
    gtk_container_add(GTK_CONTAINER(m_window), vbox);
	
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
    GtkWidget *hub = site_get_object();
    page_init(hub);
    gtk_widget_grab_focus(hub);

    // 创建主窗口
    window_create_show();

    // 创建DEBUG_WINDOW，暂时关闭
    //debug_create_show(m_window);

    gtk_main();

    // 回收资源
    site_term();

    return 0;
}
