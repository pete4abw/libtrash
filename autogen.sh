#!/bin/sh

# autogen for libtrash

[ ! -d m4 ] && mkdir m4
echo "Running Autoconf"
autoreconf -if

if [ $? -ne 0 ]; then
	echo "error: autogen could not be completed. Please review and report."
	exit -1
fi

cat >&1 <<EOF
Now run ./configure, make, and make install
CFLAGS can be set and passed to configure as in

$ CFLAGS="-O0 -g" ./configure
for debugger support, or

$ CFLAGS="-O2" ./configure
for optimization

DEBUG messages can be enabled with
$ ./configure --enable-debug...
EOF

