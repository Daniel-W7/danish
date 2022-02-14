#ifndef __PAGE_H__
#define __PAGE_H__

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <gdk/gdk.h>

//登录显示的版权信息
#define SSH_PASSWORD "password: "
#define PACKAGE     "danish"
#define VERSION     "0.04"
#define AUTHOR      "Daniel Wang"
#define EMAIL       "wanghaidi7@gmail.com"
#define COPYRIGHT   "Copyright (c) 2021-2022 " AUTHOR " <" EMAIL "> "

#ifdef __cplusplus
extern "C" {
#endif

//页面函数初始化
int page_init(GtkWidget *widget);
int page_term();

GtkWidget *page_get_notebook();

int page_get_count();

int page_get_select_num();
void page_set_select_num(int i);

int page_close(int n);
int page_close_select();

void page_set_auto_focus(int b);

int page_foreach_send_char(char c);
int page_foreach_send_string(char *str);
int page_foreach_close();

int page_send_string(int i, char *str);
int page_send_string_crlf(int i, char *str);

int page_set_title(int i, char *str);

int window_create(GtkWidget *hub_page);
#ifdef __cplusplus
};
#endif
#endif // __PAGE_H__
