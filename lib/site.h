#ifndef __SITE_H__
#define __SITE_H__

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <gdk/gdk.h>

extern const char *HOME; // $HOME
extern char PATH[256];  // $HOME + CONFIG_DIR

//配置文件路径配置
#define CONFIG_DIR    ".danioc"
#ifndef MIN
#define MIN(a,b) ((a)<=(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>=(b)?(a):(b))
#endif
#define ICON_APP    "res/icon.svg"
#define ICON_CLOSE  "res/close.png"
#define ICON_DIR    "res/dir.svg"
#define ICON_SITE   "res/site.svg"
#define ICON_SHELL  "res/shell.png"
//获取res路径
static inline char* get_res_path(const char *res)
{
    char *path = (char*) malloc(256);
    memset(path, 0x00, 256);
    sprintf(path, "%s/%s", PATH, res);
    return path;
}


static inline int str_is_endwith(char *str, int len, char *end)
{
    char *p = strstr(str, end);
    if (p &&
        (unsigned int)(len-(p-str)) == strlen(end)) {
        return 1;
    }

    return 0;
}

static inline GtkWidget *img_from_name(const char *res)
{
    char *tmp = get_res_path(res);
    GtkWidget *img = gtk_image_new_from_file(tmp);
    free(tmp);

    return img;
}

static inline GtkWidget *img_from_stock(char *id, GtkIconSize size)
{
    return gtk_image_new_from_icon_name(id, size);
}
#ifdef __cplusplus
extern "C" {
#endif

int site_init();
void site_term();

int site_load();

GtkWidget *site_get_object();

#ifdef __cplusplus
}
#endif

#endif // __SITE_H__
