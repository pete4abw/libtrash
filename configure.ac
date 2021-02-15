# configure defaults

m4_define([v_maj], [3])
m4_define([v_min], [5])

## AC_PREREQ([2.69])
AC_INIT([libtrash], [v_maj.v_min], [pete@peterhyman.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_PROG_AR
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
# only shared libraries
AC_DISABLE_STATIC
LT_INIT

# Defines for libtrash (needed because defines are set programmatically)
AH_TEMPLATE([CREAT_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([CREAT64_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([FOPEN_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([FOPEN64_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([FREOPEN_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([FREOPEN64_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([OPEN_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([OPEN64_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([RENAME_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([UNLINK_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([OPENAT_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([OPENAT64_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([RENAMEAT_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([UNLINKAT_VERSION], [Holder for GLIBC Version])
AH_TEMPLATE([AT_FUNCTIONS], [Holder for GLIBC AT Function #define])

# Defaults for compilation
# Constants YES, NO, PROTECT and ALLOW_DESTRUCTION
AC_DEFINE([YES],[1],[Default for YES])
AC_DEFINE([NO],[0],[Default for NO])
AC_DEFINE([ALLOW_DESTRUCTION],[1],[Default for ALLOW_DESTRUCTION])
AC_DEFINE([PROTECT],[0],[Default for PROTECT])
AC_DEFINE([PERSONAL_CONF_FILE],".libtrash",[Personal Configuration File])
AC_DEFINE([WARNING_STRING],"Remember that libtrash is disabled.",[Disabled Warning String])
AC_DEFINE([INTERCEPT_UNLINK],[YES],[Trap unlink])
AC_DEFINE([INTERCEPT_RENAME],[YES],[Trap rename])
AC_DEFINE([INTERCEPT_FOPEN],[YES],[Trap fopen])
AC_DEFINE([INTERCEPT_FREOPEN],[YES],[Trap freopen])
AC_DEFINE([INTERCEPT_OPEN],[YES],[Trap open])
AC_DEFINE([TRASH_CAN],"Trash",[Trash Can Directory Name])
AC_DEFINE([IN_CASE_OF_FAILURE],[PROTECT],[Failure Protection])
AC_DEFINE([SHOULD_WARN],[NO],[Remove Warning])
AC_DEFINE([PROTECT_TRASH],[YES],[Protect Trash Can Contents])
AC_DEFINE([IGNORE_EXTENSIONS],"o;log;aux",[Default Extensions NOT to preserve])
AC_DEFINE([IGNORE_HIDDEN],[YES],[Ignore Hidden Files])
AC_DEFINE([IGNORE_EDITOR_BACKUP],[YES],[Ignore Backup Files])
AC_DEFINE([IGNORE_EDITOR_TEMPORARY],[YES],[Ignore Temporary Directory])
AC_DEFINE([LIBTRASH_CONFIG_FILE_UNREMOVABLE],[YES],[Protect libtrash configuration file])
AC_DEFINE([GLOBAL_PROTECTION],[YES],[Global Protection])
AC_DEFINE([TRASH_SYSTEM_ROOT],"SYSTEM_ROOT",[Trash Directory for non HOME files])
AC_DEFINE([UNREMOVABLE_DIRS],"/bin;/boot;/dev;/etc;/lib;/lib64;/opt;/sbin;/usr",[Protect These directories])
AC_DEFINE([TEMPORARY_DIRS],"/run;/tmp;/var",[Ignore Temporary Direcgtories])
AC_DEFINE([REMOVABLE_MEDIA_MOUNT_POINTS],"/cdrom;/media;/mnt/run",[Ignore Removable Media])
AC_DEFINE([EXCEPTIONS],"/etc/mtab;/etc/resolv.conf;/etc/adjtime;/etc/upsstatus;/etc/dhcpc",[Ignore these files and allow removal])
AC_DEFINE([USER_TEMPORARY_DIRS],"",[Ignore User Temporary Directories])
AC_DEFINE([IGNORE_RE],"",[Ignore Regex])

# Debug?
AC_ARG_ENABLE(
	debug,
	[AC_HELP_STRING([--enable-debug],[Enable Debug Messages @<:@default=no@:>@])],
	debug=$enableval
       )
if test x"$debug" = xyes; then
	AC_DEFINE([DEBUG], [1], [Debug Flag])
fi


# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_SED
AC_PROG_MKDIR_P

# Checks for libraries.
AC_CHECK_LIB([dl], [dlvsym])

# Checks for header files.
AC_CHECK_HEADERS([ctype.h dlfcn.h errno.h fcntl.h pwd.h regex.h sys/stat.h \
		  stdarg.h stdlib.h string.h sys/types.h unistd.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_LANG([C])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNCS([mkdir regcomp rmdir strchr strrchr strstr])
AC_CHECK_FUNCS([creat creat64 fopen fopen64 freopen freopen64 open open64 rename unlink], ,
	       [AC_MSG_ERROR([Function `$ac_func' not found...Cannot continue])])
AC_MSG_NOTICE([Checking AT Functions])
AC_CHECK_FUNCS([openat openat64 renameat unlinkat], ,[AC_MSG_WARN([AT Function `$ac_func' not found...Ignoring])])

if test x"$ac_cv_func_openat" = "xyes" -a \
	x"$ac_cv_func_openat64" = "xyes" -a \
	x"$ac_cv_func_renameat" = "xyes" -a \
	x"$ac_cv_func_unlinkat" = "xyes" ; then
	AC_DEFINE([HAVE_ATFUNCTIONS], [1], [AT Functions Found])
	have_atfunctions=yes
else
	AC_MSG_WARN([AT Functions ignored])
fi

AC_MSG_NOTICE([Getting GLIBC Versions for libtrash functions])
AC_TRY_LINK(
[/* test program */
#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>],
[fopen("", "r");
fopen64("", "r");
freopen("", "r", NULL);
freopen64("", "r", NULL);
open("", O_RDONLY);
open64("", O_RDONLY);
creat("", S_IRWXU);
creat64("", S_IRWXU);
unlink("");
rename("", "");
#ifdef HAVE_ATFUNCTIONS
unlinkat(0, "", 0);
renameat(0, "", 0, "");
openat(0, "", 0);
openat64(0, "", 0);
#endif],
[AC_MSG_RESULT([Linking Test Program: Success]);
readelf -W --dyn-syms conftest > glibc_symbols;
sed -i -e 's/ (.*)//' glibc_symbols],
[AC_MSG_FAILURE([Link Test Program: Failure])])

AC_MSG_NOTICE([Assigning GLIBC Versions to libtrash functions])
# Read in glibc_symbols and define
for TMPVAR in creat creat64 fopen fopen64 freopen freopen64 open open64 rename unlink
do
	VERSIONVAR=$(grep -m1 $TMPVAR@ glibc_symbols | cut -d@ -f2)
	FUNCVAR=$(echo $TMPVAR | tr a-z A-Z)_VERSION
	
	if test "x$VERSIONVAR" != "x" ; then
		AC_DEFINE_UNQUOTED([$FUNCVAR],"$VERSIONVAR",[$TMPVAR Define for dlvsym call])
	else
		AC_MSG_ERROR([Cannot get GLIBC Version for $FUNCVAR...Cannot continue])
	fi
done
if test x"$have_atfunctions" = xyes; then
AC_DEFINE([AT_FUNCTIONS],[1])
for TMPVAR in openat openat64 renameat unlinkat
do
	VERSIONVAR=$(grep -m1 $TMPVAR@ glibc_symbols | cut -d@ -f2)
	FUNCVAR=$(echo $TMPVAR | tr a-z A-Z)_VERSION
	
	if test "x$VERSIONVAR" != "x" ; then
		AC_DEFINE_UNQUOTED([$FUNCVAR],"$VERSIONVAR",[$TMPVAR Define for dlvsym call])
	else
		AC_MSG_ERROR([Cannot get GLIBC Version for $FUNCVAR...Cannot continue])
	fi
done
fi

# All pre processing done

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

# Show what we have

echo "System defaults"
echo "==============="

if test x"$debug" = xyes; then
	echo "Debug Output Enabled"
fi

for VAR in PERSONAL_CONF_FILE WARNING_STRING INTERCEPT_UNLINK INTERCEPT_RENAME 		\
	INTERCEPT_FOPEN INTERCEPT_FREOPEN INTERCEPT_OPEN TRASH_CAN IN_CASE_OF_FAILURE 	\
	SHOULD_WARN PROTECT_TRASH IGNORE_EXTENSIONS IGNORE_HIDDEN IGNORE_EDITOR_BACKUP 	\
	IGNORE_EDITOR_TEMPORARY LIBTRASH_CONFIG_FILE_UNREMOVABLE GLOBAL_PROTECTION 	\
	TRASH_SYSTEM_ROOT UNREMOVABLE_DIRS TEMPORARY_DIRS REMOVABLE_MEDIA_MOUNT_POINTS 	\
	EXCEPTIONS USER_TEMPORARY_DIRS IGNORE_RE
do
	echo $(grep -m1 $VAR config.h | sed -e 's/^#define //')
done

echo "Library Functions and GLIBC Versions"
echo "===================================="

for VAR in CREAT_VERSION CREAT64_VERSION FOPEN_VERSION FOPEN64_VERSION FREOPEN_VERSION	\
	FREOPEN64_VERSION OPEN_VERSION OPEN64_VERSION RENAME_VERSION UNLINK_VERSION
do
	echo $(grep -m1 $VAR config.h | sed -e 's/^#define //')
done
if test x"$have_atfunctions" = xyes; then
for VAR in OPENAT_VERSION OPENAT64_VERSION RENAMEAT_VERSION UNLINKAT_VERSION
do
	echo $(grep -m1 $VAR config.h | sed -e 's/^#define //')
done
fi
echo "====="
echo "See directories cleanTrash and strash-0.9 for additional utilities that are not installed."
