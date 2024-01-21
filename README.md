# libtrash

## A shared library which implements a highly configurable "recycle bin" or "trash can" under GNU/Linux

Written by Manuel Arriaga (marriaga@stern.nyu.edu).

Copyright (C) 2001-2020 Manuel Arriaga
Licensed under the GNU General Public License version 2. See the file COPYING for
details.

**NOTE:** As of January 2024, Manuel has handed over the project to Peter Hyman
(pete@peterhyman.com), and the `libtrash` project will be managed on Github
https://github.com/pete4abw/libtrash .

**Version 3.8 (2024/Jan)**

## Description

libtrash is a shared library which, when preloaded, will intercept calls to
a series of GNU libc functions and make sure that, if an attempt to destroy
certain files is made, these won't be permanently destroyed but rather moved
to a "trash can".  It also allows the user to mark certain directories as
"unremovable", which means that calls to functions which would result in the
loss of files under these directories will always fail, leaving those files
untouched in their original locations.

(This last feature is meant as a higher-level substitute for ext2fs'
"immutable" flag for use by those of us who rely on other file systems. An
important difference is that libtrash allows non-privileged users to use
this with their personal files.)

The GNU libc functions which can be overriden/"wrapped" are

- unlink() / unlinkat();
- rename() / renameat();
- fopen() / fopen64();
- freopen() / freopen64();
- open() / openat() / open64() / openat64() / creat() / creat64().

You can individually enable / disable each of these "protections"; by
default, only calls to the first two functions are intercepted.

## Configuring, compiling, installing and activating libtrash

The INSTALL file contains summarized installation instructions that
most users can follow.

### Configure

`$ ./autogen.sh`
`$ ./configure [--enable-debug]`

The `--enable-debug` option should only be used by developers for testing.
It will emit a lot of output each time an unlink call is made. Other config
variables may be used for fine tuning installation, such as
* --prefix (default is **/usr/local**
* --libdir (default is **$(prefix)/lib**
* --docdir (default is **$(prefix)/share/doc/libtrash)
* --mandir (default is **$(prefix)/share/man)

NOTE: If you want to install libtrash locally as a user, set
* --prefix=$HOME

Type `./configure --help` for all configuration options available.

### Compile
`$ make`

### System-wide Install
As root, run

`$ make install`

For package maintainers, the DESTDIR variable may be used to place
installation files in a different directory. e.g.

`$ make DESTDIR=/tmp/package install` \
which will install all files under **/tmp/package** for later installation.

### User Local Install
(only if **--prefix** has been set in configure (see above))

`$ make install`

If **--prefix** has not been set, use DESTDIR

`$ make DESTDIR=$HOME install`

### User configuration file libtrash.conf
Default settings for libtrash can be overridden in the file
**$HOME/.libtrash**. A template for this file with complete descriptions
are in\
**/usr/local/etc/libtrash/libtrash.conf** or\
**/etc/libtrash.conf**

To activate user settings, copy the template libtrash.conf file to\
**$HOME/.libtrash**\
and edit accordingly.

## Activate libtrash

So that calls to the different GNU libc functions are intercepted,
you must ensure that it will be "preloaded" whenever a program is
about to get started. This is achieved by configuring your shell so
that the environment variable LD_PRELOAD is set to the path to the
libtrash library.

Assuming your system uses bash (the most popular shell on GNU/Linux
systems), that can be done by placing the following line at very
beginning ( <= IMPORTANT!) of both ~/.profile as well as ~/.bashrc:

`export LD_PRELOAD=[libdir]/libtrash.so`\
where `libdir` is the library installation directory.

Additionally, if you have access to the root account on the computer,
you probably want to make sure that you will also be covered by
libtrash while using the root account. To do so, two more steps are
necessary:

1. In ~/.bashrc, append the following lines (anywhere in that file):

    alias su="su -l"
    alias sudo="sudo -i"

2. Add the same "export LD_PRELOAD=..." line above to the very top (
<= IMPORTANT) of both /root/.profile as well as /root/.bashrc.

[Note 1: at least on my current system, for libtrash to be active
while you have sudoed into a root shell you must really place the export
LD_PRELOAD line above AT THE VERY TOP of /root/.bashrc. The reason for
that is a test that stops executing the instructions in that file for
non-interactive shells.]

[Note 2: the odds are that you don't need to export LD_PRELOAD on both
~/.bashrc and ~/.profile, but your system's shell configuration and
initialization process probably differs from mine and doing it twice
won't hurt.]

## Testing libtrash

libtrash should now be set up and ready to spare you a lot of headaches. You
can test drive it by doing the following (assuming that you didn't change
TRASH_CAN to a string other than "Trash"):

1. create a file called test_file
2. Open test_file with a text editor, type a few characters and save it.
3. or at a console prompt, type `echo test > test_file
4. Run the following commands:
```
    $ rm test_file
    $ ls Trash/
```
test_file should now be stored in $HOME/Trash/. But don't be fooled by this
example! libtrash isn't restricted to "saving" files which you explicitly
deleted with "rm": it also works with your (graphical) file manager, mail
user agent, etc...

NOTE 1: Simply "touching" a test file -- ie, "touch test_file" -- and then
removing it will no longer put it in the trash can, because libtrash now
ignores empty files, since their removal doesn't present a risk of data
loss.

NOTE 2: To make sure that you are fully covered even when sudoing into other
accounts, make sure you also test deleting a file using a command such
as "sudo rm test_file". (Notice that libtrash won't work when sudoing
unless you set the alias sudo="sudo -i" recommended above.)

## Suspending, resuming and circumventing libtrash

Should you need to temporarily disable libtrash, you can do so by running
the command

`   $ export TRASH_OFF=YES`

When you are done, libtrash can be reactivated by typing:

`   $ export TRASH_OFF=NO`

You might make these operations simpler by appending the following two lines
to the init file you used in step 5) above (if you are using Bash as your
shell):
```
    alias trash_on="export TRASH_OFF=NO"
    alias trash_off="export TRASH_OFF=YES"
```
After doing so, you can enable/disable libtrash by typing

`    $ trash_on`

or

`    $ trash_off`

at the prompt.

If you often need to remove one or more files in a definitive way using
'rm', you might wish to define

`    alias hardrm="TRASH_OFF=YES rm"`

After having done so,

`    hardrm file.txt`

will achieve the same effect as

`    TRASH_OFF=YES rm file.txt`

NOTE: I strongly advise AGAINST defining such an alias, because you will
probably get into the habit of always using it: at the time you delete a
file, you are always sure that you won't need it again... :-) The habit of
using that alias effectively makes installing libtrash useless. Unless you
wish to effectively do away with the file due to privacy/confidentiality
concerns, think instead of how cheap a gigabyte of HD space is! :-)

If you have set the option SHOULD_WARN (see libtrash.conf), running a
command while TRASH_OFF is set to "YES" will result in libtrash printing to
stderr (at least) one reminder that it is currently disabled.

**Note**: See file TLDR.md for more detailed information.

## Contact

This library was written by Manuel Arriaga. Feel free to contact me at
marriaga@stern.nyu.edu with questions, suggestions, bug reports or
just a short note saying how libtrash helped you or your organization
deploy GNU/Linux in a context where some "user friendliness" in
handling file deletions is required.
