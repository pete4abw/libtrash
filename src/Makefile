CC=gcc
CFLAGS=-Wall -W -Wmissing-prototypes -D_REENTRANT

INSTLIBDIR=/usr/local/lib
SYSCONFFILE=/etc/libtrash.conf

MAJOR =3
VERSION =3.5

all: libtrash

install: install-libtrash 

*.o: Makefile

SRC= main.c helpers.c unlink.c rename.c open-funs.c
OBJ= main.o helpers.c unlink.o rename.o open-funs.o

libtrash: libtrash.so.${VERSION}

libtrash.so.${VERSION}: $(SRC) ../libtrash.conf
	echo CHECKING FOR A WORKING PROC FILESYSTEM...
	ls -d /proc/self/fd
	echo GOOD, YOU SEEM TO HAVE A PROC FS
	$(CC) $(CFLAGS) -o linking-helper linking-helper.c
	perl -w ../scripts/trimheader.pl
	perl -w ../scripts/genheader.pl
	python ../scripts/get_symbol_versions.py
	rm -f linking-helper
	perl -w ../scripts/genconf.pl
	$(CC) $(CFLAGS) $(SRC) -nostartfiles -shared -fPIC -Wl,-soname,libtrash.so.${MAJOR} \
        -o libtrash.so.${VERSION} -ldl

install-libtrash: 
	TRASH_OFF=YES install libtrash.so.${VERSION} ${INSTLIBDIR}/libtrash.so.${VERSION}
	TRASH_OFF=YES ln -sf libtrash.so.${VERSION} ${INSTLIBDIR}/libtrash.so
	TRASH_OFF=YES install libtrash.conf.sys --mode a=r ${SYSCONFFILE}
	ldconfig	

clean:
	rm -f *~
	rm -f ../*~
	rm -f ../scripts/*~
	rm -f ../scripts/*.pyc
	rm -f ../cleanTrash/*~
	rm -f *.o
	rm -f libtrash.so.${VERSION}	
	perl -w ../scripts/trimheader.pl
	rm -f libtrash.conf.sys

