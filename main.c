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
#include "shell.h"
#include "site.h"

const char *HOME = NULL;
char PATH[256] = {0x00};

//初始化，找到家目录，并创建对应目录
int init()
{

//	HOME = NULL;
//	PATH[256] = {0x00};
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
    //读取site的配置信息
	GtkWidget *sitetree = site_get_object();
	//初始化页面
    //gtk_widget_grab_focus(sitetree);

    // 创建主窗口
    window_create(sitetree);

    // 创建DEBUG_WINDOW，暂时关闭
    //debug_create_show(window);

    gtk_main();

    // 回收资源
    site_term();

    return 0;
}
