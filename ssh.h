#ifndef __SSH_H__
#define __SSH_H__

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <gdk/gdk.h>


#define ICON_APP    "res/icon.svg"
#define ICON_CLOSE  "res/close.png"
#define ICON_DIR    "res/dir.svg"
#define ICON_SITE   "res/site.svg"
#define ICON_SHELL  "res/shell.png"

#define BTN_MAX_COUNT   16
#define CMD_MAX_COUNT   16
#define SSH_PASSWORD "password: "

typedef struct {
    char    name[256];
    char    host[256];
    char    port[256];
    char    user[256];
    char    pass[256];

} cfg_t;
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

gint page_ssh_create(cfg_t *cfg);

int run_ssh(cfg_t *cfg);

#endif // __SSH_H__
