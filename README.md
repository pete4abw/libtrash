# libtrash

## A shared library which implements a highly configurable "recycle bin" or "trash can" under GNU/Linux

Written by Manuel Arriaga (marriaga@stern.nyu.edu).

Copyright (C) 2001-2020 Manuel Arriaga
Licensed under the GNU General Public License version 2. See the file COPYING for
details.


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
* --docdir (default is /usr/share/doc/libtrash)

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
**/usr/local/share/libtrash/libtrash.conf** or\
**/usr/share/libtrash/libtrash.conf**

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

`export LD_PRELOAD=/usr/local/lib/libtrash.so`

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

## Detailed Description

This file has grown hopelessly long and verbose over the last 19
(ouch) years. Unless you are particularly curious about libtrash, users may
skip this section.

## Regarding Firefox, Chrome and other browsers based on their code:

On several systems Mozilla Firefox, Google Chrome and browsers based
on their code fail to start when libtrash is enabled. The easiest
work-around is to disable libtrash for these browsers. Do so by
starting Chrome with the command line "LD_PRELOAD= google-chrome" and
Firefox with "LD_PRELOAD= firefox" (simply adjust the command
accordingly for the other browsers). You can bind this command (if
necessary by placing it in a one-line bash script file by itself) to
whatever GUI icon or hotkey combination you use to start that browser.
You may also modify your desktop file to include LD_PRELOAD= on the
Exec= line. 

## Requirements

- (POSSIBLY) /proc filesystem: to run libtrash in most recent systems, you
need to be running a kernel with support for the /proc file system
(CONFIG_PROC_FS). Do not worry, since compilation will fail with a warning
if this requirement applies to you and you don't have it enabled.

----------------------------------------------------------------------------

**Some notes:**

1. The wrappers of the "open functions" (fopen, freopen and
open) behave differently from the real functions in an important way when
they are asked to open - in either write-mode or read-plus-write-mode - a
file considered "worthy" of having a copy of itself stored in the trash can
before being truncated: while the functions in GNU libc require
write-permission to that already existing file for the call to succeed,
these wrappers require write-permission to the DIRECTORY WHICH HOLDS THAT
FILE instead. This is so because, in fact, we are renaming the existing file
before opening it, and calls to rename() require write-permission to the
directory which holds the file for them to succeed. Usually, you only have
write-permission to files in directories to which you also have
write-permission, so this shouldn't be a huge problem in most cases.

2. When a file on a partition / filesystem other than the one
in which your trash can resides is destroyed, libtrash can't just use the
GNU libc function rename() to move it into your trash can: it must copy that
file byte-after-byte into your trash can and then delete the original. To
achieve that, read-permission to the destroyed file is required. Since in
most situations you don't have write-permission to a directory which holds
files you can't read, hopefully that won't prove a big problem, either.
However, be warned that copying a file (especially a large one) will take a
lot longer than the time which would be required to simply rename it.

3. If you are running a web (or other) server as user
'nobody', then you should ensure that libtrash is not active for that
process. The issue is that by default libtrash refuses to remove files if
it will not be able to save them in that user's trash can. The 'nobody'
account typically lacks a (writable) home directory; as such, when processes
run as 'nobody' try to remove a file, libtrash will always make that
operation fail. For that reason, always start servers run through the
'noboby' account (or any other, 'low privileges' account missing a writable
home dir) with libtrash disabled (just prefix "LD_PRELOAD= " to the command
line). (My thanks to Nicola Fontana for pointing this out!)

----------------------------------------------------------------------------

libtrash works with any GNU/Linux program, both at the console as well
as in graphical windowing environments, and operates independently of
the programming language the program was written in. The only
exception are statically linked programs, which you probably won't
find a lot of. It can be extensively configured by each user through a
personal, user-specific configuration file.

Although libtrash itself was written in C, the installation procedure
requires both Perl and Python (sorry!).

## How libtrash works / features 

libtrash recreates the directory structure of your home directory under the
trash can, which means that, should you need to recover the mistakenly
deleted file /home/user/programming/test.c, it will be stored in
/home/user/Trash/programming/test.c. If you have instructed libtrash to also
store copies of files which you delete in directories outside of your home
dir (see libtrash.conf for details), they will be available under
Trash/SYSTEM_ROOT. E.g.: after deletion by the user joe, /common-dir/doc.txt
will be available at /home/joe/Trash/SYSTEM_ROOT/common-dir/doc.txt.

When you try to delete a file in a certain directory where you had
previously deleted another file with the same name, libtrash stores the new
file with a different name, in order to preserve both files. E.g.:
```
    $ echo test >test
    $ rm test 
    $ ls Trash/ test 
    $ touch test 
    $ rm test 
    $ ls Trash/ 
    test test[1] <-- The file we deleted first wasn't lost. 
```

libtrash keeps generating new names until no name collision occurs. The
deletion of a file never causes the permanent loss of a previously "deleted"
file.

Temporary files can be automatically "ignored" and really destroyed.

You can define whether you wish to allow the definitive removal of files
already in your trash can while libtrash is active. Allowing this has one
major disadvantage, which is explained in libtrash.conf. But, on the other
hand, if you don't allow the destruction of files already in your trash can,
when you need to recover HD space by permanently removing files currently
found in your trash can you will have to temporarily disable libtrash first
(instructions on how to achieve this can be found below). 

To avoid the accumulation of useless files in your users' trash cans, it is
probably wise to run the script cleanTrash regularly (perhaps from a cron
job). This Perl script was kindly provided by Daniel Sadilek and works by
removing the oldest files from each trash can in your system whenever that
trash can grows beyond a certain disk size. It is meant to be run by root. 
cleanTrash, together with the license according to which it can be
distributed and a short README file written by me, can be found under the
directory "cleanTrash".

You can also choose whether files outside of your home directory, hidden
files (and files under hidden directories), backup and temporary files used
by text editors and files on removable media should be handled normally or
"ignored" (i.e., you can decide if copies of such files should be created in
your trash can if the originals are about to be destroyed). It is also
possible to discriminate files based on their names' extensions: you can
instruct libtrash, e.g., to always ignore object files (files with the
extension ".o"), meaning that deleting files of this type won't result in
the creation of copies of these in your trash can. By default, besides
object files, TeX log and auxiliary files (".log" and ".aux", respectively)
are also ignored.

The user may also configure libtrash to print a warning message to stderr
after each "dangerous" function call while libtrash is disabled. This
feature is meant to remind the user that libtrash is disabled and that, for
that reason, any deletions will be permanent. This feature is disabled by
default, so that libtrash remains "invisible" to the user.

Other options: name of trash can, name of TRASH_SYSTEM_ROOT under your trash
can, whether to allow the destruction of the configuration file used by
libtrash, and what to do in case of error. You can also set in your
environment a list of exceptions to the list of unremovable directories.

To configure libtrash so that it better suits your purposes you should edit
the file libtrash.conf before compiling. Even if you won't be configuring
libtrash at compile-time, it is recommended that you at least read
config.txt so that you know how libtrash handles its configuration files.


## Limitations

1. As mentioned in the second note near the top of this document, destroying
documents in a partition different from the one on which your home directory
resides will result in a byte-after-byte copy of those files into your trash
can. If I could think of a faster, more efficient way of doing that I would
have already implemented it. Unfortunately, things like quota-support and
access permissions make this problem hard to solve. Unless someone suggests
a good way to overcome this, this isn't going to change any time soon...
Sorry!

2. As mentioned in the section on how to activate libtrash, the
LD_PRELOAD method doesn't protect you from mistakes while using an
account in which (for any reason) LD_PRELOAD isn't set and pointing to
libtrash. If you 'su' or 'sudo' into other accounts, you should (i)
make sure that, by default, they have LD_PRELOAD pointing to libtrash
and (ii) always use "su - [user name]", instead of "su [user name]".
The only alternative is to activate libtrash from /etc/ld.so.preload,
which I don't recommend since it can get in the way of important
system updates.

## Contact

This library was written by Manuel Arriaga. Feel free to contact me at
marriaga@stern.nyu.edu with questions, suggestions, bug reports or
just a short note saying how libtrash helped you or your organization
deploy GNU/Linux in a context where some "user friendliness" in
handling file deletions is required.

## Credits

- Avery Pennarun, whose "freestyle-concept" tarball showed me how to
intercept function calls and write a suitable Makefile.

- Phil Howard and wwp for pointing out problems with the (abandoned) hardrm
script. wwp also offered general advice.

- Karl Pitrich for letting me know about a bug in the calls to mkdir() and
chmod() in the code of dir_ok() which rendered the trash can (and all
subdirs) unbrowsable if you didn't manually correct their permissions with
'chmod'.

- Daniel Sadilek for letting me know that some people _did_ need
inter-device support :), the helpful cleanTrash Perl script and help testing
libtrash-0.6.

- Ross Skaliotis for helping me pin down the cause of the incompatibility
between libtrash and Samba.

- Christoph Dworzak for reporting poor handling of special files, and
providing a patch.

- Martin Corley for the alternative cleanTrash script.

- Frederic Connes for reporting a bug affecting the creation of replacement
files when open()/open64() are being intercepted and suggesting the use of
different names for 5 configuration variables. Frederic also provided two
patches against the Makefile and cleanTrash script.

- Dan Stutzbach for reporting a bug in the function readline(), sending a
patch and helping me fix it.

- Ryan Brown for reporting a memory leak.

- BBBart for reporting a bug in the handling of files with names starting
with multiple dots.

- Jacek Sliwerski (rzyjontko) for adding the IGNORE_RE feature to libtrash and
making the search for new filenames [in reformulate_new_path()] much faster.

- Robert Storey for reporting an error in the documentation.

- Raik Lieske for reporting a bug in the handling of certain file paths.

- David Benbennick for reporting mishandling of function calls with a NULL
pathname.

- Jorgen Schaefer for helping me figure out why glibc was crashing when
libtrash was active and getwc() was called.

- Philipp Woelfel for alerting me to lacking coverage of the *at() functions
and allowing me access to his system to find out what was going on. Also,
Philipp spotted the "unresolved symbol" bug in 2.7.

- Nicola Fontana for pointing out the problem with servers running as
'nobody'.

- Peter Hyman for pointing out that the default value of
REMOVABLE_MEDIA_MOUNT_POINTS was outdated.

- Kamil Dudka for sending me a patch renaming the _init/_fini
functions to avoid symbol clashes when using Audacious plugins.

- Peter Hyman (again :) ) for letting me know that Google Chrome (and
not just some versions of Firefox) also need to be launched with
'LD_PRELOAD='.

- Felix Becker for informing me about Kate, Lyx and tar creating files
with oddly restrictive permissions when libtrash was being used.

- Peter Hyman for submitting a patch adding PRESERVE_FILES_LARGER_THAN
functionality.

- Peter Hyman for single-handedly bringing libtrash to the 21st (20th?
:-) ) century by replacing the whole clunky build system with autotools
magic.
