## Process this file with automake to produce Makefile.in

bin_PROGRAMS = danioc

#源文件
danioc_SOURCES = danioc-gtk.c danioc.c

#c预处理参数,需要调用的库函数目录
AM_CPPFLAGS = $(GTK_CFLAGS) $(PTHREAD_CFLAGS) -I/usr/include/vte-2.91/

#连接时需要用到的库文件
danioc_LDADD = $(GTK_LIBS)\
            $(PTHREAD_LIBS)\
            $(EXTRA_LIBS)\
            $(GTHREAD_LIBS)\
            $(SSL_LIBS)\
            $(LIBINTL)
#只用来编译，不用放到安装目录下的头文件
noinst_HEADERS = danioc.h
