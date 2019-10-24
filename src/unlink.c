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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#define __USE_ATFILE 1 /* for access to AT_REMOVEDIR/AT_FDCWD macros inside fcntl.h */ 
#include <fcntl.h>
#include <sys/stat.h>

#include "trash.h"

static int unlink_handle_error(const char *pathname, int (*real_unlink) (const char*),
			       int in_case_of_failure);

/* What this version of unlink() does: if everything is OK, we just rename() the given file instead of
 * unlink()ing it, putting it under absolute_trash_can.
 *
 * We also take care so that, if we fail, errno is either zero (if a
 * "libtrash-specific" error occurred) or has a meaningful value which the
 * caller should know how to interpret after a call to unlink(). This way,
 * we avoid confusing the caller with a errno set by some other GNU libc
 * function used by the libtrash wrapper.
 *
 */

int unlink(const char *pathname)
{
   struct stat path_stat;
   
   char *absolute_path = NULL;
   
   int symlink = 0;
   
   int error = 0;
   
   int retval = 0;
   
   int file_should = 0;
   
   /* Create a config structure, in which all configuration settings will be placed: */
   
   config cfg;
   
#ifdef DEBUG
   fprintf(stderr, "\nEntering unlink().\n");
#endif
   
   /* Run libtrash_init(), which sets all the global variables: */
   
   libtrash_init(&cfg);
   
   /* We always call libtrash_fini() before quitting. */
   
   /* Isn't a pointer to GNU libc's unlink() available? In that case, there's nothing we can do: */
   
   if (!cfg.real_unlink)
     {
#ifdef DEBUG
	fprintf(stderr, "real_unlink unavailable. unlink() returning error code.\n");
#endif
	errno = 0;
	
	libtrash_fini(&cfg);
	return -1; /* errno set to 0 in order to avoid confusing the caller. */
     }
   
   /* If libtrash_off is set to true or intercept_unlink set to false, the user has asked us to become temporarily inactive an let the real
    * unlink() perform its task.
    Alternatively, if we were passed a NULL pointer we also call the real unlink: */
   
   if (cfg.libtrash_off || !cfg.intercept_unlink || pathname == NULL)
     {
#ifdef DEBUG
	if (!pathname)
	  fprintf(stderr, "unlink was passed a NULL pointer, calling real unlink.\n");
	else
	  fprintf(stderr, "Passing request to unlink %s to the real unlink because libtrash_off = true or intercept_unlink = false.\n", pathname);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_unlink) (pathname); /* real unlink() sets errno */
     }
   
   /* If general_failure is set, something went wrong while initializing and we should just invoke the real function: */
   
   if (cfg.general_failure)
     {
#ifdef DEBUG
	fprintf(stderr, "general_failure is set in unlink(), invoking unlink_handle_error().\n"
		"in_case_of_failure has value %d.\n", cfg.in_case_of_failure);
#endif
	libtrash_fini(&cfg);
	return unlink_handle_error(pathname, cfg.real_unlink, cfg.in_case_of_failure); /* If in_case_of_failure is set to PROTECT, we return -1 with errno set to 0;
											otherwise, the real unlink() sets errno. */
     }
   
   /* First of all: has the user mistakenly asked us to remove either a missing file, a special file or a directory?
    * In any of these cases we, just let the normal unlink() complain about it and save ourselves the
    * extra trouble: */
   
   error = lstat(pathname, &path_stat);
   
   if (( error && errno == ENOENT)              ||
       (!error && S_ISDIR(path_stat.st_mode))   ||
       (!error && !S_ISREG(path_stat.st_mode) && !S_ISLNK(path_stat.st_mode)) )
     {
#ifdef DEBUG
	fprintf(stderr, "%s either doesn't exit, or is a special file (non-symlink) or is a directory.\nCalling the \"real\" unlink().\n", pathname);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_unlink) (pathname); /* real unlink() sets errno. */
     }
   
   /* If this is a symlink, we set symlink. We don't call the real unlink() immediately because we don't remove
    * symlinks under protected directories:
    */
   
   if (S_ISLNK(path_stat.st_mode))
     symlink = YES;
   else
     symlink = NO;
   
   /* Let us begin by building an absolute path, if we weren't passed one: */
   
   /* [If no error occurred, the memory absolute_path now points to was dynamically allocated.
    That is the reason why we free absolute_path before returning.] */
   
   /* We have a problem: if the pathname contains references to symlinks to directories (e.g.,
    /tmp/syml/test, where /tmp/syml is a link to /var/syml, the real
    canonical path being /var/syml/test), we want to resolve those (because
    the real unlink follows them), but if the filename in pathname (i.e.,
    the last "component" - "test", in the example above) is a symlink
    itself, then we DON'T want to resolve it because the real unlink will
    delete it, not the file it points at. For that reason, rather than
    directly invoking canonicalize_file_name(), we call
    build_absolute_path(), which takes care of these complications for us:
    */
   
   absolute_path = build_absolute_path(pathname, 0);
   
   if (!absolute_path)
     {
#ifdef DEBUG
	fprintf(stderr, "Unable to build absolute_path.\nInvoking unlink_handle_error().\n");
#endif
	libtrash_fini(&cfg);
	return unlink_handle_error(pathname, cfg.real_unlink, cfg.in_case_of_failure); /* about errno: the same as in the other call to unlink_handle_error() above. */
     }
   
   /* Independently of the way in which the argument was written, absolute_path now holds the absolute
    * path to the file, free of any "/../" or "/./" and with the any symlinks (except for the filename itself,
    if it is one) resolved.
    */
   
#ifdef DEBUG
   fprintf(stderr, "CLEANSED ABSOLUTE_PATH:   |%s|\n", absolute_path);
#endif
   
   /* By now we want to know whether this file "qualifies" to be stored in the trash can rather than deleted (another
    possible option is this file being considered "unremovable"). This decision is taken according to the user's preferences
    by the function decide_action(): */
   
   file_should = decide_action(absolute_path, &cfg);
   
   switch (file_should)
     {
	
      case BE_REMOVED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told unlink() to permanently destroy file %s.\n", absolute_path);
#endif
	
	retval = (*cfg.real_unlink) (pathname); /* real unlink() sets errno. */
	
	break;
	
      case BE_LEFT_UNTOUCHED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told unlink() to leave file %s untouched.\n", absolute_path);
#endif
	
	retval = -1;
	
	/* Set errno to EACCESS, which the caller will interpret as "insufficient permissions": */
	
	errno = EACCES;
	
	break;
	
      case BE_SAVED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told unlink() to save a copy of file %s.\n", absolute_path);
#endif
	
	/* But if it is a symlink we refuse to "save" it nonetheless: */
	
	if (symlink)
	  {
#ifdef DEBUG
	     fprintf(stderr, " but its suggestion is being ignored because %s is just a symlink.\n", absolute_path);
#endif
	     retval = (*cfg.real_unlink) (pathname); /* real unlink() sets errno. */
	  }
	else
	  {
	     /* (See below for information on this code.) */
	     
	     /* see (0) */
	     if (found_under_dir(absolute_path, cfg.home))
	       retval = graft_file(cfg.absolute_trash_can, absolute_path, cfg.home, &cfg);
	     else
	       retval = graft_file(cfg.absolute_trash_system_root, absolute_path, NULL, &cfg);
	     
	     if (retval == -2) /* see (1) */
	       retval = -1;
	     else           /* see (2) */
	       errno = 0;
	     
#ifdef DEBUG
	     fprintf(stderr, "graft_file(), called by unlink(), returned %d.\n", retval);
#endif
	  }
	
	break;
	
     }
   
   /* Notes on case BE_SAVED:
    *
    *
    * (0) if the file lies under the user's home directory, we "graft" it
    * into the user's trash can; if the file lies outside of the user's
    * home directory and global_protection is set we graft it to the
    * directory SYSTEM_ROOT under the user's trash can.
    *
    *
    * (1) graft_file() failed due to the lack of write-access (either
    * to the directory which holds this file or to the file itself
    * - the latter if we tried to "manually" move() the file). In
    * these situations, we leave errno untouched because it
    * contains meaningful information (EACCES, EPERM or EROFS),
    * just converting retval to -1 (which is the correct error code
    * for unlink()
    *
    * (2) if graft_file() either succeeded or failed for some other reason:
    * just zero errno and return retval, which contains a valid (and correct)
    * error code.
    */
   
   /* Free memory before quitting: */
   
   free(absolute_path);
   
   libtrash_fini(&cfg);
   return retval;
}


/* This wrapper is less "faithful" done the other ones. Rather than invoking
 * the real GNU libc unlinkat(), it simply invokes our "doctored" unlink() or 
 * (the real) rmdir() depending on the value of its 'flags' argument. Ie, the
 * real unlinkat() is never used. See important note in the CHANGE.LOG about 
 * our wrapping of the *at() functions. */

#ifdef AT_FUNCTIONS

int unlinkat(int dirfd, const char *arg_pathname, int flags);

int unlinkat(int dirfd, const char *arg_pathname, int flags)
{
   int retval = 0;
   
   char *real_path = make_absolute_path_from_dirfd_relpath(dirfd, arg_pathname);
   
   if (real_path == NULL)
     {
	return -1; /* XXX this exit should also be handled according to the rule specified by in_case_of_failure; for the time being we just "fail" */
     }
   
   if (flags & AT_REMOVEDIR)
     {
	retval = rmdir(real_path); /* invoke the REAL (we don't implement a doctored one anyway!) rmdir() and set retval */
     }
   else
     {
	/* invoke our own unlink; all the libtrash wrapping madness occurs there, no need to duplicate it here */
	retval = unlink(real_path);
     }
   
   if (real_path != arg_pathname)
     {
	free(real_path); /* free mem allocated by canonicalize_file_name() above */
     }
   
   return retval;
}

#endif

/* ------------------------------------------------------------------------------------ */


/* This function is called by unlink() in case an error is spotted. It does what the user
 * told us to do in these situations (through the argument in_case_of_failure), and unlink() just
 * returns the value passed by unlink_trash_error():
 */

static int unlink_handle_error(const char *pathname, int (*real_unlink) (const char*), int in_case_of_failure)
{
   if (in_case_of_failure == ALLOW_DESTRUCTION)
     return (*real_unlink) (pathname); /* the real unlink() sets errno */
   else /* if (in_case_of_failure == PROTECT) */
     {
	errno = 0;
	return -1; /* errno is set to zero to avoid confusing the caller. */
     }
   
}

