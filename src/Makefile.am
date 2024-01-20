MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libtrash.la
libtrash_la_SOURCES = \
	main.c \
	helpers.c \
	open-funs.c \
	rename.c \
	unlink.c \
	trash.h

AM_CFLAGS=-nostartfiles -D_REENTRANT

libtrash_la_LDFLAGS = -version-number $(LT_VER)
