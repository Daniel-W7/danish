#ifndef __SSH_H__
#define __SSH_H__

#include <gtk/gtk.h>
#include <vte/vte.h>

typedef struct {
    char    name[256];
    char    host[256];
    char    port[256];
    char    user[256];
    char    pass[256];

} cfg_t;

typedef struct {
    enum {
        PG_TYPE_HUB,
        PG_TYPE_SSH,
        PG_TYPE_SHELL,
    } type;

    // head
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
#ifdef __cplusplus
extern "C" {
#endif

#define SSH_PASSWORD "password: "

gint page_ssh_create(cfg_t *cfg);

int run_ssh(pg_t *pg);

#ifdef __cplusplus
};

#endif

#endif // __SSH_H__
