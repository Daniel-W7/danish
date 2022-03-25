## Process this file with automake to produce Makefile.in

bin_PROGRAMS = danioc

danioc_SOURCES = danioc-gtk.c danioc.c

AM_CPPFLAGS = $(GTK_CFLAGS) $(PTHREAD_CFLAGS)

LDADD = ../../lib/libdanioc.a ../uicommon/libgftpui.a $(GTK_LIBS) $(PTHREAD_LIBS) $(EXTRA_LIBS) $(GTHREAD_LIBS) $(SSL_LIBS) $(LIBINTL)

noinst_HEADERS = danioc.h
