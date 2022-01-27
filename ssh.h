#ifndef __SSH_H__
#define __SSH_H__

#define SSH_PASSWORD "password: "

typedef struct {
    char    name[256];
    const char *host;
    int    port;
    char    user[256];
    char    pass[256];

} cfg_t;

int run_ssh(cfg_t *cfg);

#endif // __SSH_H__
