#ifndef __CONFIG_H__
#define __CONFIG_H__

#define PACKAGE     "danioc"
#define VERSION     "0.06"
#define AUTHOR      "Daniel Wang"
#define EMAIL       "wanghaidi7@gmail.com"
#define COPYRIGHT   "Copyright (c) 2021-2022 " AUTHOR " <" EMAIL "> "
#include "../../lib/ssh.h"
#include "../../lib/site.h"


#define CONFIG_DIR    ".danioc"
extern const char *HOME; // $HOME
extern char PATH[256];  // $HOME + CONFIG_DIR

#define ICON_APP    "res/icon.svg"
#define ICON_CLOSE  "res/close.png"
#define ICON_DIR    "res/dir.svg"
#define ICON_SITE   "res/site.svg"
#define ICON_SHELL  "res/shell.png"



#endif // __CONFIG_H__
