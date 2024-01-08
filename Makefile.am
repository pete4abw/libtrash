
ACLOCAL_AMFLAGS = -I m4
MAINTAINERCLEANFILES = \
	Makefile.in \
	aclocal.m4 \
	ar-lib \
	compile \
	config.guess \
	config.h.in \
	config.sub \
	configure \
	depcomp \
	install-sh \
	ltconfig \
	ltmain.sh \
	missing \
	$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz \
	$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.bz2 \
	$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-doc.tar.bz2 \
	m4/*

DISTCLEANFILES = glibc_symbols

SUBDIRS = src man

dist_doc_DATA = \
	AUTHORS \
	BUILD \
	ChangeLog \
	config.txt \
	INSTALL \
	NEWS \
	README.md \
	TODO

dist_sysconf_DATA = libtrash.conf

EXTRA_DIST =  $(dist_doc_data)
