## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libdanioc.a

//源文件
libdanioc_a_SOURCES= \
	cache.c \
	charset-conv.c \
	config_file.c \
	fsplib.c \
	ftp-dir-listing.c \
	misc.c \
	protocols.c \
	protocol_bookmark.c \
	protocol_fsp.c \
	protocol_ftp.c \
	protocol_ftps.c \
	protocol_localfs.c \
	protocol_http.c \
	protocol_https.c \
	pty.c \
	sshv2.c \
	sslcommon.c \
	socket-connect.c \
	sockutils.c \
	site.cpp \
	ssh.c

AM_CPPFLAGS= \
	$(GLIB_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	-DSHARE_DIR=\"$(datadir)/danioc\" \
	-DLOCALE_DIR=\"$(datadir)/locale\"

//头文件
noinst_HEADERS= \
	danioc.h \
	protocol_ftp.h \
	protocol_http.h \
	options.h \
	fsplib.h \
	site.h \
	ssh.h
