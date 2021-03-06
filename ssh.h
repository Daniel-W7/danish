#ifndef __SSH_H__
#define __SSH_H__

#include <gtk/gtk.h>
#include <vte/vte.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSH_PASSWORD "password: "

typedef struct {
    char    name[256];
    char    str[256];
} cmd_t;

typedef struct {
    char    name[256];
    cmd_t   cmd[CMD_MAX_COUNT];
} btn_t;

typedef struct {
    char    name[256];
    char    host[256];
    char    port[256];
    char    user[256];
    char    pass[256];

    btn_t   btn[BTN_MAX_COUNT];
} cfg_t;

typedef struct {
    //定义枚举type
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
void *wait_ssh_child(void *p);
void run_shell(pg_t *pg);
void run_ssh(pg_t *pg);

#ifdef __cplusplus
}
#endif

#endif // ___H__
