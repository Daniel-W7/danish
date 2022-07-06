#ifndef __DANIOCGTK_H__
#define __DANIOCGTK_H__

#include "../../lib/ssh.h"

#ifdef __cplusplus
extern "C" {

#endif

#define SSH_PASSWORD "password: "
gint page_shell_create();
gint page_ssh_create(cfg_t *cfg);

int page_init(GtkWidget *sitetree);

int window_create(GtkWidget *sitetree);

#ifdef __cplusplus
};
#endif

#endif // __DANIOCGTK_H__
