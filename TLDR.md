**Note**: January 2024. This file is extracted from the original README. 

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
