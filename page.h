#ifndef __PAGE_H__
#define __PAGE_H__

#include <gtk/gtk.h>
#include <vte/vte.h>
#include "ssh.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSH_PASSWORD "password: "

typedef struct {
   
    struct {
        GtkWidget   *box;
         GtkWidget   *image;
        GtkWidget   *label;
        GtkWidget   *button;
    } head;

    // body
    GtkWidget   *body;

    union {
        struct {
        } hub;
        
        struct {
            GtkWidget *vte;
            VtePty  *pty;
            pid_t   child;
            int     need_stop;
            cfg_t   cfg;
        } ssh;

        struct {
            GtkWidget *vte;
            VtePty  *pty;
            pid_t   child;
        } shell;
        
    };

} pg_t;

int page_init(GtkWidget *widget);
int page_term();

GtkWidget *page_get_notebook();

gint page_ssh_create(cfg_t *cfg);
gint page_shell_create();

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
