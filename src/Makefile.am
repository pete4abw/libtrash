MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libtrash.la
libtrash_la_SOURCES = \
	main.c \
	helpers.c \
	open-funs.c \
	rename.c \
	unlink.c \
	trash.h

AM_CFLAGS=-nostartfiles -Wall -W -Wmissing-prototypes -D_REENTRANT

libtrash_la_LDFLAGS = -version-number ${VERSION//./:}
