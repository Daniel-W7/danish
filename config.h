#ifndef __CONFIG_H__
#define __CONFIG_H__

#define PACKAGE     "danish"
#define VERSION     "0.01"
#define AUTHOR      "Daniel Wang"
#define EMAIL       "wanghaidi7@gmail.com"
#define COPYRIGHT   "Copyright (c) 2021-2022 " AUTHOR " <" EMAIL "> "

#define CONFIG_DIR    ".danish"

extern const char *HOME; // $HOME
extern char PATH[256];  // $HOME + CONFIG_DIR

#define ICON_APP    "res/icon.svg"
#define ICON_CLOSE  "res/close.png"
#define ICON_DIR    "res/dir.svg"
#define ICON_SITE   "res/site.svg"
#define ICON_SHELL  "res/shell.png"

#define BTN_MAX_COUNT   16
#define CMD_MAX_COUNT   16

#endif // __CONFIG_H__
