/* Copyright 2001, 2002, 2003, 2004, 2005, 2006, 2007 Manuel Arriaga
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* This file defines the wrappers for the GNU libc functions fopen(), freopen() and
 * open(). Since they are all pretty much the same, they all invoke a function
 * called do_fopen_or_freopen_or_open() which emulates each of these
 * functions according to its first argument, a flag which indicates which
 * function it should behave as.
 *
 * do_fopen_or_freopen_or_open() returns a value of type FdOrFp (see below),
 * from which the caller function (fopen(), freopen() or open()) extracts the
 * correct member, according to the type of its own return value (f(re)open()
 * return (FILE*), open() int).
 *
 *  We also take care so that, if we fail, errno is either zero (if a
 * "libtrash-specific" error occurred) or has a meaningful value which the
 * caller should know how to interpret after a call to rename(). This way,
 * we avoid confusing the caller with a errno set by some other GNU libc
 * function used by the libtrash wrapper.
 *
 */

/* Notes:
 *
 *
 * When a program calls fopen(), freopen() or open() with arguments which
 * don't imply opening the file in write-(or read+write)mode and truncate
 * it if that file already exists, or if that file doesn't need to be saved
 * in the trash can according to the user's preferences, these functions
 * behave in exactly the same way as the corresponding GNU libc functions
 * do (actually, they do nothing besides calling these). On the other hand,
 * if the program calls one of these functions with arguments which specify
 * opening a file in write-(or read+write)mode and truncating it if that
 * file already exists, and that file already exists and is considered
 * "worthy" of having a copy of itself stored in the trash can (again,
 * according to the user's preferences), these functions only succeed if
 * the user has write-permission to the directory under which the file
 * resides, while the original GNU libc functions succeed if the user has
 * write-permission to the file itself.
 *
 * Does this pose any security risk? NOT AT ALL: whenever you have
 * write-permission to a directory, you are able to delete any file in that
 * directory and create new (empty) files with the exact same names as the
 * previously existing files had. This is exactly what libtrash does in
 * these situations: rather than asking the system to open and truncate a
 * file (-> requires write-permission to the file itself), it removes
 * (actually, it rename()s) the file and then creates a new, empty file
 * with the same name (-> requires write-permission to the directory). THIS
 * CAN ALWAYS BE DONE MANUALLY WHENEVER YOU HAVE WRITE-PERMISSION TO A
 * DIRECTORY.
 */

#define _GNU_SOURCE /* for access to canonicalize_file_name() inside stdlib.h
		       and the constant O_NOFOLLOW inside fcntl.h */
#define __USE_ATFILE 1 /* for access to AT_REMOVEDIR/AT_FDCWD macros inside fcntl.h */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "trash.h"

/* This is the return type of the "magic" function, do_fopen_or_freopen_or_open(): */

typedef union
{
	int fd;
	FILE *fp;
}
FdOrFp;

/* This is the prototype of the "magic" function: */

static FdOrFp do_fopen_or_freopen_or_open(int function, const char *path, ...);

/* -------------------- */

/* These are the definitions of the wrappers for the six glibc functions we override: */

FILE* fopen(const char *path, const char *mode)
{
	FdOrFp retval = do_fopen_or_freopen_or_open(FOPEN, path, mode);
	return retval.fp; /* returns a file pointer */
}

FILE* fopen64(const char *path, const char *mode)
{
	FdOrFp retval = do_fopen_or_freopen_or_open(FOPEN64, path, mode);
	return retval.fp; /* returns a file pointer */
}

FILE* freopen(const char *path, const char *mode, FILE *stream)
{
	FdOrFp retval = do_fopen_or_freopen_or_open(FREOPEN, path, mode, stream);
	return retval.fp; /* returns a file pointer */
}

FILE* freopen64(const char *path, const char *mode, FILE *stream)
{
	FdOrFp retval = do_fopen_or_freopen_or_open(FREOPEN64, path, mode, stream);
	return retval.fp; /* returns a file pointer */
}

int open(const char *path, int flags, ...)
{
	FdOrFp retval;
	/* If (flags & O_CREAT) or (flags & O_TMPFILE), then a third argument should be available: */
	if (flags & O_CREAT || flags & O_TMPFILE)
	{
		va_list arg_list;
		mode_t mode;
		va_start(arg_list, flags);
		mode = va_arg(arg_list, mode_t);
		va_end(arg_list);
		retval = do_fopen_or_freopen_or_open(OPEN, path, flags, mode);
	}
	else /* we don't need to get a third argument: */
		retval = do_fopen_or_freopen_or_open(OPEN, path, flags);

	return retval.fd; /* return an integer (file descriptor) */
}

int open64(const char *path, int flags, ...)
{
	FdOrFp retval;
	/* If (flags & O_CREAT) or (flags & O_TMPFILE), then a third argument should be available: */
	if (flags & O_CREAT || flags & O_TMPFILE)
	{
		va_list arg_list;
		mode_t mode;
		va_start(arg_list, flags);
		mode = va_arg(arg_list, mode_t);
		va_end(arg_list);
		retval = do_fopen_or_freopen_or_open(OPEN64, path, flags, mode);
	}
	else /* we don't need to get a third argument: */
		retval = do_fopen_or_freopen_or_open(OPEN64, path, flags);

	return retval.fd; /* return an integer (file descriptor) */
}

/* I had missed these two when I first implemented the wrappers around open(64)(). Since
 * the man page for them says they are "equivalent" to the corresponding open() function
 * with flags set to O_CREAT|O_WRONLY|O_TRUNC, we save some trouble by merely redirecting
 * these calls to the right wrapper defined above. */

int creat(const char *pathname, mode_t mode)
{
	return open(pathname, O_CREAT | O_WRONLY| O_TRUNC, mode);
}

int creat64(const char *pathname, mode_t mode)
{  
	return open64(pathname, O_CREAT | O_WRONLY| O_TRUNC, mode);
}

#ifdef AT_FUNCTIONS

int openat(int dirfd, const char *arg_pathname, int flags, ...)
{
	int retval = 0;
	char *real_path = make_absolute_path_from_dirfd_relpath(dirfd, arg_pathname);
	if (real_path == NULL)
	{
		return -1; /* XXX this exit should also be handled according to the rule specified by in_case_of_failure; for the time being we just "fail" */
	}
	/* invoke our own open(); all the libtrash wrapping madness occurs there, no need to duplicate it here */
	/* If (flags & O_CREAT) or (flags & O_TMPFILE), then a third argument should be available: */
	if (flags & O_CREAT || flags & O_TMPFILE)
	{
		va_list arg_list;
		mode_t mode;
		va_start(arg_list, flags);
		mode = va_arg(arg_list, mode_t);
		va_end(arg_list);
		retval = open(real_path, flags, mode);
	}
	else /* we don't need to get a third argument: */
	{
		retval = open(real_path, flags);
	}

	if (real_path != arg_pathname)
	{
		free(real_path); /* free mem allocated by canonicalize_file_name() above */
	}

	return retval;
}

/* See important comment above (on top of open()). */

int openat64(int dirfd, const char *arg_pathname, int flags, ...)
{
	int retval = 0;
	char *real_path = make_absolute_path_from_dirfd_relpath(dirfd, arg_pathname);
	if (real_path == NULL)
	{
		return -1; /* XXX this exit should also be handled according to the rule specified by in_case_of_failure; for the time being we just "fail" */
	}
	/* invoke our own open64(); all the libtrash wrapping madness occurs there, no need to duplicate it here */
	/* If (flags & O_CREAT) or (flags & O_TMPFILE), then a third argument should be available: */

	if (flags & O_CREAT || flags & O_TMPFILE)
	{
		va_list arg_list;
		mode_t mode;
		va_start(arg_list, flags);
		mode = va_arg(arg_list, mode_t);
		va_end(arg_list);
		retval = open64(real_path, flags, mode);
	}
	else /* we don't need to get a third argument: */
	{
		retval = open64(real_path, flags);
	}

	if (real_path != arg_pathname)
	{
		free(real_path); /* free mem allocated by canonicalize_file_name() above */
	}

	return retval;
}

#endif


/* ---------------------------------------------------------------------------- */

/* Macros: */

/* ---------------------------------------------------------------------- */

/* This macro is only used if we are running in DEBUG mode: it points function_name to the name of the function
 * we are emulating for use in diagnostics messages (the code isn't enclosed in brackets because the pointers
 * must be accessible to the rest of this function): */

#define DETERMINE_FUNCTION_NAME()		\
	if (function == FOPEN)			\
		function_name = fopen_name;	\
	else if (function == FOPEN64)		\
		function_name = fopen64_name;	\
	else if (function == FREOPEN)		\
		function_name = freopen_name;	\
	else if (function == FREOPEN64)		\
		function_name = freopen64_name;	\
	else if (function == OPEN)		\
		function_name = open_name;	\
	else if (function == OPEN64)		\
		function_name = open64_name;	\

/* -------------------------------------------------------------------- */

/* This (uglier!) macro extracts the arguments that are missing given the function we are running as: */

#define EXTRACT_MISSING_ARGS()						\
{									\
	va_start(arg_list, path);					\
									\
	if (function == FOPEN || function == FREOPEN ||			\
			function == FOPEN64 || function == FREOPEN64)	\
		mode_str = va_arg(arg_list, char*);			\
	if (function == FREOPEN || function == FREOPEN64)		\
		stream = va_arg(arg_list, FILE*);			\
									\
	if (function == OPEN || function == OPEN64)			\
	{								\
		flags = va_arg(arg_list, int);				\
		if (flags & O_CREAT || flags & O_TMPFILE)		\
		mode = va_arg(arg_list, mode_t);			\
	}								\
									\
	va_end(arg_list);						\
}

/* -------------------------------------------------------------------------------- */

/* This macro makes the function return the appropriate error code for the function we are running as, and sets errno
   to the value err (in most cases, it will be zero in order to avoid confusing the caller with meaningless errno values
   set by other library functions used by this meta-wrapper): */

#define RETURN_FUNCTION_ERROR(err)							\
{											\
	FdOrFp retval;									\
	if (function == FOPEN || function == FREOPEN ||					\
			function == FOPEN64 || function == FREOPEN64)			\
	retval.fp = NULL;								\
	else /* if (function == OPEN || function == OPEN64)*/				\
	retval.fd = -1;									\
	errno = err;									\
	return retval; /* the caller will know which member of the union to access */ 	\
}

/* --------------------------------------------------------------------------------- */

/* This is an ugly macro which invokes the real function with the right arguments and
 * evaluates to its return value: (Note: "real function" refers to the
 * function wrapped by the libtrash function which called us.) */

#define REAL_FUNCTION()										\
	(function == FOPEN ? (*real_fopen) (path, mode_str) :					\
	 (function == FOPEN64 ? (*real_fopen64) (path, mode_str) :				\
	  (function == FREOPEN ? (*real_freopen) (path, mode_str, stream) :			\
	   (function == FREOPEN64 ? (*real_freopen64) (path, mode_str, stream) :		\
	    (function == OPEN ?									\
	     ( (flags & O_CREAT || flags & O_TMPFILE) ? (FILE *) (intptr_t) (*real_open) (path, flags, mode) :	\
	       (FILE *) (intptr_t) (*real_open) (path, flags) ) :							\
	       ( (flags & O_CREAT || flags & O_TMPFILE) ? (FILE *) (intptr_t) (*real_open64) (path, flags, mode) :	\
		 (FILE *) (intptr_t) (*real_open64) (path, flags) ) ) ) ) ) )

/* ------------------------------------------------------------------------------------- */

/* This macro makes the function return the value returned by the function it is emulating, and lets the errno
   value be defined by that function: */

#define RETURN_REAL_FUNCTION()						\
{									\
	FdOrFp retval;							\
	if (function == FOPEN || function == FREOPEN ||			\
			function == FOPEN64 || function == FREOPEN64)	\
		retval.fp = REAL_FUNCTION();				\
	else /* if (function == OPEN || function == OPEN64) */		\
		retval.fd = (intptr_t) REAL_FUNCTION();			\
	return retval;							\
}

/* --------------------------------------------------------------------------------------- */

/* This macro evaluates to the intercept_* variable matching the function we are running
as: */

#define REAL_FUNCTION_OFF()								\
	((function == FOPEN || function == FOPEN64) ? !cfg.intercept_fopen :		\
	 ((function == FREOPEN || function == FREOPEN64) ? !cfg.intercept_freopen :	\
	  !cfg.intercept_open) )

/* ------------------------------------------------------------------------------------------ */

/* This macro is equivalent to the unlink_handle_error() and rename_handle_error() in the other two components
 * of libtrash. It checks the value of the configuration variable cfg.in_case_of_failure and acts accordingly, also
 * asking RETURN_FUNCTION_ERROR() to set the variable errno to zero in order to avoid confusing the caller: */

#define DO_HANDLE_ERROR()					\
{								\
	if(cfg.in_case_of_failure == ALLOW_DESTRUCTION)		\
		RETURN_REAL_FUNCTION()				\
	else /* if (cfg.in_case_of_failure == PROTECT) */	\
		RETURN_FUNCTION_ERROR(0);			\
}

/* ----------------------------------------------------------------------------------------------- */

/* This macro evaluates to TRUE if the function we are emulating was called in a mode which allows
 * truncating the specified file, to zero otherwise: */

#define WRITE_TRUNCATE_MODE_ENABLED()							\
	( ( (function == FOPEN || function == FREOPEN ||				\
	     function == FOPEN64 || function == FREOPEN64) && mode_str[0] == 'w' ) ||	\
	     ( (function == OPEN || function == OPEN64) && !(flags & O_TMPFILE) &&	\
	       (flags & O_WRONLY || flags & O_RDWR) && (flags & O_TRUNC) ) )

/* ----------------------------------------------------------------------------------------------- */

/* This macro runs either lstat() or stat() on the file we were passed as an argument and then checks
 * whether (a) it exists, (b) it is actually a file, rather than a directory, (c) it is a _regular_ file or symlink (not a char dev, FIFO, etc) and (d) if we are emulating
 * open() and flags has O_NOFOLLOW set, whether path points to a "real" file rather than to a symlink.
 * If any of these checks fails, it invokes the real function and returns its return value.
 *
 * stat() is used in most cases because all these three functions normally "follow"/resolve symlinks in the
 * path they are passed, the exception being the behaviour of open() when the option O_NOFOLLOW is set in
 * the argument flags: in that particular case, the real open() will fail if path is the name of a symlink
 * rather than of an actual file. For that reason, if ((function == OPEN) && (flags & O_NOFOLLOW)), we use
 * the function lstat() instead so that we can detect symlinks.
 */

#define TEST_FILE_EXISTENCE()										\
{													\
	if ( (function == OPEN || function == OPEN64) && (flags & O_NOFOLLOW) )				\
		error = lstat(path, &file_stat);							\
	else												\
		error = stat(path, &file_stat);								\
													\
	if ( (error && errno == ENOENT)								||	\
		(!error && S_ISDIR(file_stat.st_mode))						||	\
		(!error && !S_ISREG(file_stat.st_mode) && !S_ISLNK(file_stat.st_mode)) 		||	\
		(!error && (function == OPEN || function == OPEN64) 				&&	\
		 (flags & O_NOFOLLOW) && S_ISLNK(file_stat.st_mode) ) ) 				\
	{												\
		libtrash_fini(&cfg);									\
		RETURN_REAL_FUNCTION();									\
	}												\
}

/* ----------------------------------------------------------------------------------------- */

/* This macro does some open()-specific preparation before the call to the real open() in case we have successfully
 * moved the original file to the trash can. The rationale is the following: if the calling program knew that this
 * file already existed, it probably didn't set O_CREAT in the flags and, therefore, didn't specify a mode to use
 * when creating the file. But now that we have surreptitiously moved this file to another location, the real open()
 * will need to create a new file if it is going to succeed. For that reason, if the user didn't originally set
 * O_CREAT in flags, we need to do it ourselves so that the call to the real open() results in the (re)creation of
 * that file and that function succeeds. But, when instructing open() to create a new file if necessary, we also need
 * to tell it which mode to use when creating it. We will instruct open() to create a new file with the same
 * permissions (mode) which the original one had: */

#define DO_OPEN_SPECIFIC_PREPARATION()			\
{							\
	if (function == OPEN || function == OPEN64)	\
	{						\
		flags |= O_CREAT;			\
		mode = file_stat.st_mode;		\
	}						\
}

/* After this code snippet is executed, the macro RETURN_REAL_FUNCTION() will see that (flags & O_CREAT) and
 * conclude that it needs to pass a third argument - mode - to the real open(), and will do so, telling open()
 * to create a file the same permissions as the original one had.
 *
 fopen() and freopen() don't need this preparation because both functions, when invoked in write-mode, automatically
 create a file if it doesn't already exist. */

/* ------------------------------------------------------------------------ */



/* This macro retrieves a pointer to the glibc version of the function we are running as. If it fails,
 * we return an error code: (the code isn't delimited by brackets because the pointers need to be available
 * to the rest of the function) */

#define GET_REAL_FUNCTION()									\
	FILE* (*real_fopen) (const char *path, const char *mode) = NULL;			\
	FILE* (*real_fopen64) (const char *path, const char *mode) = NULL;			\
	FILE* (*real_freopen) (const char *path, const char *mode, FILE *stream) = NULL;	\
	FILE* (*real_freopen64) (const char *path, const char *mode, FILE *stream) = NULL;	\
	int (*real_open) (const char *path, int flags, ...) = NULL;				\
	int (*real_open64) (const char *path, int flags, ...) = NULL;				\
												\
	if (function == FOPEN && !( real_fopen = get_real_function(function) ) ) 		\
		error = 1; 									\
	else if (function == FOPEN64 && !( real_fopen64 = get_real_function(function) ) ) 	\
		error = 1; 									\
	else if (function == FREOPEN && !( real_freopen = get_real_function(function) ) ) 	\
		error = 1; 									\
	else if (function == FREOPEN64 && !( real_freopen64 = get_real_function(function) ) ) 	\
		error = 1; 									\
	else if (function == OPEN && !( real_open = get_real_function(function) ) ) 		\
		error = 1; 									\
	else if (function == OPEN64 && !( real_open64 = get_real_function(function) ) ) 	\
		error = 1; 									\
												\
	if (error)										\
	RETURN_FUNCTION_ERROR(0);								\


/* ----------------------------------------------------------------------------- */

/* We now define the "magic" function: */

static FdOrFp do_fopen_or_freopen_or_open(int function, const char *path, ...)
{

	/* The following four variables are used to hold the missing arguments: */

	char *mode_str = NULL;		/* used by fopen() and freopen() */
	FILE *stream = NULL;		/* used only by freopen() */
	mode_t mode;			/* used only by open() */
	int flags = 0;			/* used only by open() */
	/* Other variables: */
	va_list arg_list;
	struct stat file_stat;
	int error = 0;
	char *absolute_path = NULL;
	int file_should = 0;

#ifdef DEBUG
	const char
		*fopen_name   = "fopen",   *fopen64_name = "fopen64",
		*freopen_name = "freopen", *freopen64_name = "freopen64",
		*open_name    = "open",    *open64_name = "open64";

	const char *function_name = NULL;
#endif

	config cfg;

	/* This function is different from unlink() and rename(), because it gets invoked by glibc functions called by libtrash_init().
	 * In order to avoid an infinite loop, we mustn't call libtrash_init() in those situations, but rather invoke the real open
	 * function. For that reason, this function needs to get a pointer to the "real function" it is playing all by itself
	 * (that is one of the jobs libtrash_init() does for the other functions).
	 * How does this function avoid calling libtrash_init() when it has been called by one of the glibc functions called by libtrash_init()?
	 * Fortunately, those functions only need to open files in read-only mode, so we can simply quit before calling libtrash_init()
	 * if we were asked to open a file in read-mode, thereby avoiding this problem. If, in the future, the glibc functions
	 * called by libtrash_init need to open files in write-mode libtrash in its current state will become unusable. I really can't
	 * think of a better (and thread-safe) solution.
	 */

	/* First, get a pointer to the real functions: (if this fails, we quit because
	 * there's nothing else we can do) */
	GET_REAL_FUNCTION();

	/* Get the missing arguments and store them in the above mentioned variables: */
	EXTRACT_MISSING_ARGS();

	/* If DEBUG is defined, we need to know the name of the function we are emulating: */
#ifdef DEBUG
	DETERMINE_FUNCTION_NAME();
	fprintf(stderr, "Entering do_fopen_or_freopen_or_open(). Arg: %s.\n", path);
#endif

	/* Do our arguments specify write-mode with orders to truncate existing files? If they don't (or if we were passed a NULL argument), we have nothing
	 * to do:
	 */
	if (!WRITE_TRUNCATE_MODE_ENABLED() || path == NULL)
	{

#ifdef DEBUG
		fprintf(stderr, "Passing request to the real %s because the arguments we were passed don't specify "
				"write-mode with orders to truncate existing files (or we have a NULL file name).\n", function_name);
#endif
		RETURN_REAL_FUNCTION(); /* we don't need to call libtrash_fini() here. */
	}

	/* First we call libtrash_init(), which will set the configuration variables: */
	libtrash_init(&cfg);

	/* From this point on, we always call libtrash_fini() before quittting. */
	/* Is cfg.libtrash_off set to true or cfg.intercept_(real_function) set to false? If so, just invoke the real function: */
	if (cfg.libtrash_off || REAL_FUNCTION_OFF())
	{
#ifdef DEBUG
		fprintf(stderr, "Passing request to the real function because libtrash_off = true or intercept_%s = false.\n", function_name);
#endif
		libtrash_fini(&cfg);
		RETURN_REAL_FUNCTION();
	}

	/* Is cfg.general_failure set? If so, invoke the error handler: */
	if (cfg.general_failure)
	{
#ifdef DEBUG
		fprintf(stderr, "Invoking %s_handle_error() because general_failure is set.\n", function_name);
#endif
		libtrash_fini(&cfg);
		DO_HANDLE_ERROR();
	}

	/* Does this file already exist? If it doesn't (or if path is the path to a directory, or to a symlink and we
	 * are operating as open() and the O_NOFOLLOW is set in the flags), we invoke the real function because no
	 * data loss can occurr. This test, like all other function-specific tests in this "meta-function", is encapsulated
	 * in a macro, which invokes the real function and returns its return value if any of those conditions is true: */

#ifdef DEBUG
	fprintf(stderr, "Doesn't file %s exist?...\n", path);
#endif

	TEST_FILE_EXISTENCE(); /* Invokes libtrash_fini() by itself if it decides to return. */

#ifdef DEBUG
	fprintf(stderr, "It does exist.\n");
#endif

	/* From this point on we need the absolute canonical path to the file called path; we invoke
	 * canonicalize_file_name() (unlike unlink() and rename(), these three functions follow all symlinks in
	 * the path, including the filename itself, if it is one): */
	absolute_path = canonicalize_file_name(path); /* We can use canon...() directly, rather than build_absolute_path(),
							 because by now we are sure that a file called path exists and we don't
							 mind following a symlink in the last component of the path (unlike what
							 happens in unlink.c and rename.c). */

	if (!absolute_path)
	{
#ifdef DEBUG
		fprintf(stderr, "Unable to build absolute_path\nInvoking DO_HANDLE_ERROR() inside %s.\n", function_name);
#endif
		libtrash_fini(&cfg);

		DO_HANDLE_ERROR();
	}

	/* ++++++++++++++++ */

	/* Independently of the way in which the argument was written, absolute_newpath now holds the absolute
	 * path to the file, free of any "/../" or "/./" and with all symlinks resolved. */

	/* We now need to decide whether to warrant protection to this file which is about to be truncated; we
	 * do so by invoking the function decide_action() and analysing its return value: */
	file_should = decide_action(absolute_path, &cfg);

	switch (file_should)
	{

		case BE_REMOVED: /* free memory, call real function and return its return value: */

#ifdef DEBUG
			fprintf(stderr, "decide_action() told %s() to permanently destroy file %s.\n", function_name, absolute_path);
#endif
			free(absolute_path);
			libtrash_fini(&cfg);
			RETURN_REAL_FUNCTION();
			break;
		case BE_LEFT_UNTOUCHED: /* free memory, return error code and DON'T call real function: */
#ifdef DEBUG
			fprintf(stderr, "decide_action() told %s() to leave %s untouched and return an error code.\n", function_name,
					absolute_path);
#endif
			free(absolute_path);
			libtrash_fini(&cfg);
			RETURN_FUNCTION_ERROR(EACCES); /* setting errno to EACCES so that the caller interprets this error as being due to
							* "insufficient permissions" */
			break;
		case BE_SAVED: /* move file into trash can (with graft_file()), free memory and either invoke real function or error */
			/* handler, deciding what to do according to the return value of graft_file(): */
#ifdef DEBUG
			fprintf(stderr, "decide_action() told %s() to save a copy of %s in the trash can and then invoke the real function.\n",
					function_name, absolute_path);
#endif
			if (found_under_dir(absolute_path, cfg.home))
				error = graft_file(cfg.absolute_trash_can, absolute_path, cfg.home, &cfg);
			else
				error = graft_file(cfg.absolute_trash_system_root, absolute_path, NULL, &cfg);

			free(absolute_path);
			if (error) /* graft_file() failed, look at in_case_of_failure and decide what to do: */
			{
#ifdef DEBUG
				fprintf(stderr, "graft_file() failed, %s() invoking DO_HANDLE_ERROR().\n", function_name);
#endif
				libtrash_fini(&cfg);
				DO_HANDLE_ERROR();
			}
			else /* if graft_file() succeeded and the file is safely stored, proceed with the call to the real function: */
			{
#ifdef DEBUG
				fprintf(stderr, "graft_file() succeded, %s() invoking the real function.\n", function_name);
#endif
				/* If we are running as open(), we need to do some open()-specific preparation (see macro definition
				   for more details): */
				DO_OPEN_SPECIFIC_PREPARATION();
				libtrash_fini(&cfg);
				RETURN_REAL_FUNCTION();
			}
			break;
	}
	/* (No return statement because all cases in the above switch-statement contain one.) */
}
