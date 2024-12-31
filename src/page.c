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

#include "page.h"
#include "site.h"

//gtk初始化组件
//添加gtk组件

GtkWidget *window;
GtkWidget *hbox;//横向窗口
GtkWidget *vbox;//纵向窗口
GtkWidget *sidebar;
GtkWidget *stack;
GtkWidget *notebook;
GtkWidget *menu;
GtkWidget *menu_copy;
GtkWidget *menu_paste;
GtkWidget *menu_copy_paste;
static int m_auto_focus = 1;
//获取打开的标签数
int page_get_count()
{
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
}
//设置当前选择的页面的标签
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
    //获取打开的所有的标签
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), n);
    //获取打开页面的类型
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
    if (pg->type == PG_TYPE_SSH) {
        kill(pg->ssh.child, SIGKILL);
    }
    if (pg->type == PG_TYPE_SHELL) {
        kill(pg->shell.child, SIGKILL);
    }

    return 0;
}
/*
//定义关闭选中页面
int page_get_select_num()
{
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
}
*/
int page_close_select()
{
    int page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    return page_close(page_num);
}

int page_set_title(int i, char *str)
{
    return -1;
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
//关闭对应page
static void on_close_clicked(GtkWidget *widget, gpointer user_data)
{
    pg_t *pg = (pg_t*) user_data;
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pg->body);
    page_close(num);
}
static void on_menu_copy_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");

    VteTerminal *vte = (VteTerminal*) NULL;
    if (pg->type == PG_TYPE_SHELL) {
        vte = (VteTerminal*) pg->shell.vte;
    }
    else if (pg->type == PG_TYPE_SSH) {
        vte = (VteTerminal*) pg->ssh.vte;
    }
    vte_terminal_copy_clipboard_format(vte, VTE_FORMAT_TEXT);
    //vte_terminal_copy_clipboard(vte);//‘vte_terminal_copy_clipboard’ is deprecated
    //warning: implicit declaration of function ‘vte_terminal_select_none’; did you mean ‘vte_terminal_select_all’?
    //vte_terminal_select_none(vte);
}
static void on_menu_paste_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");

    VteTerminal *vte = (VteTerminal*) NULL;
    if (pg->type == PG_TYPE_SHELL) {
        vte = (VteTerminal*) pg->shell.vte;
    }
    else if (pg->type == PG_TYPE_SSH) {
        vte = (VteTerminal*) pg->ssh.vte;
    }

    vte_terminal_paste_clipboard(vte);
}

static void on_menu_copy_paste_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
    pg_t *pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");

    VteTerminal *vte = (VteTerminal*) NULL;
    if (pg->type == PG_TYPE_SHELL) {
        vte = (VteTerminal*) pg->shell.vte;
    }
    else if (pg->type == PG_TYPE_SSH) {
        vte = (VteTerminal*) pg->ssh.vte;
    }
    vte_terminal_copy_clipboard_format(vte, VTE_FORMAT_TEXT);
    //vte_terminal_copy_clipboard(vte);
    //warning: implicit declaration of function ‘vte_terminal_select_none’; did you mean ‘vte_terminal_select_all’?
    //vte_terminal_select_none(vte);
    vte_terminal_paste_clipboard(vte);
}

// 标签选中改变时
// 1、修改标签颜色
// 2、使标签内vte获得焦点
static void on_notebook_switch(GtkNotebook *notebook, GtkWidget *page,
                               guint page_num, gpointer user_data)
{
    // 修改标签颜色
    //
    // 未被选中为黑色，被选中为红色
    //GdkColor color;

    pg_t *pg = NULL;
    //gdk_color_parse("black", &color);//warning: ‘gdk_color_parse’ is deprecated: Use 'gdk_rgba_parse' instead
    int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    int i = 0;
    for (i = 1; i<count; i++) {
        GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
        pg = (pg_t*) g_object_get_data(G_OBJECT(p), "pg");
        if (pg->head.label) {
            //warning: ‘gtk_widget_modify_fg’ is deprecated: Use 'gtk_widget_override_color' instead    
            //gtk_widget_modify_fg(pg->head.label, GTK_STATE_NORMAL, &color); 
        }
    }

    pg = (pg_t*) g_object_get_data(G_OBJECT(page), "pg");
    //warning: ‘gdk_color_parse’ is deprecated: Use 'gdk_rgba_parse' instead
    //gdk_color_parse("red", &color);
    if (pg->head.label) {
        //warning: ‘gtk_widget_modify_fg’ is deprecated: Use 'gtk_widget_override_color' instead    
        //gtk_widget_modify_fg(pg->head.label, GTK_STATE_NORMAL, &color);
    }

    // 移动焦点到vte上
    if (m_auto_focus) {
        gtk_widget_grab_focus(page);
    }
}
// 右键显示菜单
static gboolean on_vte_button_press(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    VteTerminal *vte = (VteTerminal*) widget;
    GdkEventButton *button = (GdkEventButton*) event;

    if (button->type == GDK_BUTTON_PRESS && // 按下
        button->button == 3) {  // 右键

        gtk_widget_set_sensitive(menu_copy, FALSE);
        gtk_widget_set_sensitive(menu_copy_paste, FALSE);
        gtk_widget_set_sensitive(menu_paste, FALSE);

        // copy / copy_paste
        if (vte_terminal_get_has_selection(vte)) {
            gtk_widget_set_sensitive(menu_copy, TRUE);
            gtk_widget_set_sensitive(menu_copy_paste, TRUE);
        }

        // paste
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (clipboard && gtk_clipboard_wait_is_text_available(clipboard)) {
            gtk_widget_set_sensitive(menu_paste, TRUE);
        }
        
        // popup menu
        gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    }

    return FALSE;
}

static int menu_create()
{
    int row = 0;
    GtkWidget *mi = NULL;

    menu = gtk_menu_new();
    
    menu_copy  = gtk_menu_item_new_with_label("Copy");
    //warning: ‘GtkStock’ is deprecated [-Wdeprecated-declarations]
    //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_copy),img_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));

    //gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_copy), TRUE);
    g_signal_connect(G_OBJECT(menu_copy), "activate", G_CALLBACK(on_menu_copy_clicked), NULL);
    gtk_menu_attach(GTK_MENU(menu), menu_copy, 0, 1, row, row+1);
    row++;
    menu_paste  = gtk_menu_item_new_with_label("Paste");
    //warning: ‘GtkStock’ is deprecated [-Wdeprecated-declarations]
    //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_paste),img_from_stock(GTK_STOCK_PASTE, GTK_ICON_SIZE_MENU));
    //gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_paste), TRUE);
    g_signal_connect(G_OBJECT(menu_paste), "activate", G_CALLBACK(on_menu_paste_clicked), NULL);
    gtk_menu_attach(GTK_MENU(menu), menu_paste, 0, 1, row, row+1);
    row++;

    menu_copy_paste = gtk_menu_item_new_with_label("Copy & Paste");
    g_signal_connect(G_OBJECT(menu_copy_paste), "activate", G_CALLBACK(on_menu_copy_paste_clicked), NULL);
    gtk_menu_attach(GTK_MENU(menu), menu_copy_paste, 0, 1, row, row+1);
    row++;

    // separator
    mi = gtk_separator_menu_item_new();
    gtk_menu_attach(GTK_MENU(menu), mi, 0, 1, row, row+1);
    row++;

    mi = gtk_menu_item_new_with_label("Close");
    //warning: ‘GtkStock’ is deprecated [-Wdeprecated-declarations]
    //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
    //gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(mi), TRUE);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(page_close_select), NULL);
    gtk_menu_attach(GTK_MENU(menu), mi, 0, 1, row, row+1);
    row++;

    gtk_widget_show_all(menu);

    return 0;
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
    int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pg->body);
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
    //warning: implicit declaration of function ‘vte_terminal_set_pty_object’
    //vte_terminal_set_pty_object((VteTerminal*)pg->shell.vte, pg->ssh.pty);
    //warning: implicit declaration of function ‘vte_terminal_set_font_from_string’
    //vte_terminal_set_font_from_string((VteTerminal*)pg->shell.vte, "WenQuanYi Micro Hei Mono 11");
    vte_terminal_set_scrollback_lines((VteTerminal*)pg->shell.vte, 1024);
    vte_terminal_set_scroll_on_keystroke((VteTerminal*)pg->shell.vte, 1);
    g_object_set_data(G_OBJECT(pg->shell.vte), "pg", pg);
    g_signal_connect(G_OBJECT(pg->shell.vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);

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

    // cfg,配置文件
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
    //未定义，暂时禁用，此项会导致ssh窗口无法打开，无法进行远程连接,vte2.91需要将vte_pty_new改为vte_pty_new_sync
    pg->ssh.pty = vte_pty_new_sync(VTE_PTY_DEFAULT, NULL,NULL); 
    vte_terminal_set_pty((VteTerminal*)vte, pg->ssh.pty);
    vte_terminal_set_font_scale((VteTerminal*)vte, 1.5);//定义pty终端缩放的大小
    vte_terminal_set_scrollback_lines((VteTerminal*)vte, 1024);
    //vte_terminal_set_scroll_on_keystroke((VteTerminal*)vte, 1);
    g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(on_vte_button_press), NULL);

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
//页面显示初始化配置，配置显示shell还是ssh连接的内容
int page_init(GtkWidget *sitetree) 
{
    // popup menu
    menu_create();

    //配置显示配置
    pg_t *pg = (pg_t*) malloc(sizeof(pg_t));
    bzero(pg, sizeof(pg_t));
    pg->type = PG_TYPE_HUB;
   
    return 0;
}
//创建窗口
int window_create(GtkWidget *sitetree)
{

	pg_t *pg = (pg_t*) malloc(sizeof(pg_t));

	//定义notebook显示的label
    pg->head.label = gtk_label_new("Sessions");
	
    //初始化页面，并配置右键菜单
    page_init(sitetree);  

    // body,定义页面主要内容
	//pg->body = sitetree;

	// notebook,创建notebook
	notebook = gtk_notebook_new();
	//定义切换notebook页面的操作
	g_signal_connect_after(G_OBJECT(notebook), "switch-page", G_CALLBACK(on_notebook_switch), NULL);
	//将body里面的内容连接到Sessions label下面
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pg->body, pg->head.label);
        
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
    	//定义hbox，横向显示，左侧用于放置站点信息，右侧用于放置shell和ssh终端
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);   
        
       	//定义侧边栏站点树显示
		//gtk_container_add(GTK_CONTAINER(hbox), sitetree);
        gtk_box_pack_start(GTK_BOX(hbox),sitetree,FALSE,FALSE,0);
        //定义ssh连接notebook显示
		gtk_box_pack_start(GTK_BOX(hbox),notebook,TRUE,TRUE,0);
	
	//将hbox添加到window里面，显示两个组件
	gtk_container_add(GTK_CONTAINER(window), hbox);

	gtk_widget_set_events(window, GDK_BUTTON_PRESS_MASK|GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_window_key_press), NULL);
    	//定义退出按钮
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all(window);
	//gtk_main();
	return 0;
}
