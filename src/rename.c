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
#include <errno.h>

#include <sys/stat.h>

#include "trash.h"


static int rename_handle_error(const char *oldpath, const char *newpath, 
			       int (*real_rename) (const char*, const char*), int in_case_of_failure);

/* When can rename() cause data loss? 
 * When both its arguments refer to existing files, then the original file
 * at newpath will be lost.
 * 
 * What this version of rename() does: if a file already exists at newpath, we move it 
 * (with the "real" rename()) to the trash can and then hand over control to GNU libc's 
 * rename().
 * 
 * We also take care so that, if we fail, errno is either zero (if a
 * "libtrash-specific" error occurred) or has a meaningful value which the
 * caller should know how to interpret after a call to rename(). This way,
 * we avoid confusing the caller with a errno set by some other GNU libc
 * function used by the libtrash wrapper.
 * 
 */

int rename(const char *oldpath, const char *newpath)
{
   
   struct stat path_stat;
   
   char *absolute_newpath = NULL;
   
   int symlink = NO;
   
   int error = 0;
   
   int retval = 0;
   
   int file_should = 0;
   
   config cfg;
   
#ifdef DEBUG
   fprintf(stderr, "\nEntering rename().\n");
#endif
   
   
   /* First we call libtrash_init(), which will set the configuration variables: */
   
   libtrash_init(&cfg);
   
   /* We always call libtrash_fini() before quitting. */
   
   /* If real_rename is unavailable, we must return -1 because there's nothing else we can do: */
   
   if (!cfg.real_rename)
     {
#ifdef DEBUG
	fprintf(stderr, "real_rename is unavailable.\nrename returning -1.\n");
#endif
	errno = 0; /* we set errno to zero so that, if errno was previously set to some other value, it 
		    doesn't confuse the caller. */
	libtrash_fini(&cfg);
	return -1;
     }
   
   
   /* If libtrash_off is set to true or intercept_rename is set to false, the user has asked us to become temporarily inactive an let the
    * real rename() perform its task. Alternatively, we might have been passed a NULL pointer and let the real rename handle that:*/
   
   if (cfg.libtrash_off || !cfg.intercept_rename || 
       oldpath == NULL || newpath == NULL)
     {
#ifdef DEBUG
	fprintf(stderr, "Passing request to rename(%s, %s) to the real rename() because libtrash_off = true or intercept_rename = false (OR: one of the args is NULL).\n", 
		oldpath, newpath);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_rename) (oldpath, newpath); /* real rename() sets errno. */
     }
   
   
   
   /* If general_failure is set, we know that something went wrong in _init, and we do whatever in_case_of_failure
    * specifies, returning the appropriate error code.*/
   
   if (cfg.general_failure)
     {     
#ifdef DEBUG
	fprintf(stderr, "general_failure is set in rename(), invoking rename_handle_error().\n"
		"in_case_of_failure has value %d.\n", cfg.in_case_of_failure);
#endif
	libtrash_fini(&cfg);
	return rename_handle_error(oldpath, newpath, cfg.real_rename, cfg.in_case_of_failure); /* either the real rename() sets errno, or we return -1 and errno is 
												* set to 0. */
     }
   
   
   /* First of all: does a regular file called newpath already exist? If it doesn't, we don't need to 
    * do anything: 
    */
   
   error = lstat(newpath, &path_stat);
   
   if ((error && errno == ENOENT)             ||
       (!error && S_ISDIR(path_stat.st_mode)) ||
       (!error && !S_ISREG(path_stat.st_mode) && !S_ISLNK(path_stat.st_mode)) )
     {
#ifdef DEBUG
	fprintf(stderr, "newpath (%s) either doesn't exit, or is a special file (non-symlink) or is a directory.\nCalling the \"real\" rename().\n", newpath);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_rename) (oldpath, newpath); /* errno set by real rename(). */
     } 
   
   
   /* We don't protect symbolic links, except if they are under one of the protected directories; therefore, we must
    * postpone the decision about whether to ignore a file because it is a symlink until we know if it is "protected":
    */
   
   if (!error && S_ISLNK(path_stat.st_mode))
     symlink = YES;
   else
     symlink = NO;
   
   
   /* Second: does a file called oldpath exist? If it doesn't (or if it is a dir), there's nothing for us to do, because
    the real call to rename() will necessarily fail (explanation: if oldpath is a dir, we can be sure that the rename()
    will fail because a _file_ called newpath already exists, and rename() doesn't overwrite files so that it can
    successfully rename dirs):
    */
   
   error = lstat(oldpath, &path_stat);
   
   if ( (error && errno == ENOENT) || (!error && S_ISDIR(path_stat.st_mode)) )
     {
#ifdef DEBUG
	fprintf(stderr, "oldpath (%s) either  doesn't exist or is a directory.\nCalling the \"real\" rename().\n", oldpath);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_rename) (oldpath, newpath); /* errno set by real rename() */
     }
   
   
   
   /* Even if both files exist, are we sure that we have write-access 
    * to the directory the file called oldpath is found in? If we don't
    * have it (either due to lacking write-permission or due to the fact
    * that oldpath points to a file on a read-only filesystem), then the
    * call to the "real" rename() will fail and we will end up just moving
    * the file presently at newpath to the trash can, unless we exit at
    * this point.
    * 
    * How might this happen? If we can write to the dir in which newpath is
    * found, but not to the one which holds oldpath, we will successfully
    * move the file presently found at newpath to the trash can and then
    * once again invoke rename() to move the file called oldpath from
    * oldpath to newpath; but this second call to rename() will fail and
    * the user will end up with the file originally called newpath in the
    * trash can, and the file called old_path in the same place.
    * 
    * For this reason, we call the real rename() right away and let it
    * (fail and) complain about insufficient permissions / a RO FS.
    * 
    * Two notes on can_write_to_dir(): (1st) if it fails due to an internal
    * error, it returns TRUE, so that rename() doesn't exit at this point
    * (it behaves this way because, if an error occurrs, we prefer to make
    * an unnecessary copy of a file over the possibility of NOT making a
    * NECESSARY one) (2nd) can_write_to_dir() performs the permissions
    * check with the _effective_ UID rather than the real one, so that
    * SETUID programs work correctly (of course, we are assuming that the
    * program already did the appropriate permission checks -that's its
    * responsability). */
   
   if (!can_write_to_dir(oldpath))
     {
#ifdef DEBUG
	fprintf(stderr, "We don't have write-access to the dir which contains oldpath (%s).\n"
		"Calling the \"real\" rename().\n", oldpath);
#endif
	libtrash_fini(&cfg);
	return (*cfg.real_rename) (oldpath, newpath); /* errno set by real rename() */
     }
   
   
   
   /* By now we know that our services are needed: rename(oldpath, newpath) might cause the loss of the file originally
    * called newpath, because both exist and we have write-permission to the dirs which contain them.
    */
   
   
   /* Let us begin by building an absolute path, if we weren't passed one: */
   
   /* [If no error occurred, memory was dynamically allocated and it is now pointed to by absolute_newpath. 
    That is the reason why we free absolute_newpath before returning.] */
   
   /* We don't use GNU libc's canonicalize_file_name() directly for reasons explained in unlink.c: */
   
   absolute_newpath = build_absolute_path(newpath, 0);
   
   if (!absolute_newpath)
     {
#ifdef DEBUG
	fprintf(stderr, "Unable to build absolute_newpath.\nInvoking rename_handle_error().\n");
#endif
	libtrash_fini(&cfg);
	return rename_handle_error(oldpath, newpath, cfg.real_rename,
				   cfg.in_case_of_failure); /* errno set either by the real rename() or set to 0 (just like the other
							     * call to rename_handle_error() above). */
     }
   
   
   
   /* Independently of the way in which the argument was written, absolute_newpath now holds the absolute
    * path to the new filename, free of any "/../" or "/./". */
   
#ifdef DEBUG
   fprintf(stderr, "CLEANSED ABSOLUTE_NEWPATH:   |%s|\n", absolute_newpath);
#endif
   
   
   /* +++++++++++++ */
   
   /* By now we want to know whether the file at newpath "qualifies" to be stored in the trash can rather than
    permanently lost (another possible option is this file being considered "unremovable"). This decision is 
    taken according to the user's preferences by the function decide_action(): */
   
   
   file_should = decide_action(absolute_newpath, &cfg);
   
   switch (file_should)
     {
	
      case BE_REMOVED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told rename() to permanently destroy file %s.\n", absolute_newpath);
#endif
	
	retval = (*cfg.real_rename) (oldpath, newpath); /* errno set by real rename() */
	
	break;
	
	
      case BE_LEFT_UNTOUCHED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told rename() to leave file %s untouched.\n", absolute_newpath);
#endif
	
	/* Set errno so that the caller interprets this failure as "insufficient permissions": */
	
	errno = EACCES;
	
	retval = -1;
	
	break;
	
	
      case BE_SAVED:
	
#ifdef DEBUG
	fprintf(stderr, "decide_action() told rename() to save a copy of file %s.\n", absolute_newpath);
#endif
	
	/* But if it is a symlink we refuse to "save" it nonetheless: */
	
	if (symlink)
	  {
#ifdef DEBUG
	     fprintf(stderr, " but its suggestion is being ignored because %s is just a symlink.\n", absolute_newpath);
#endif
	     retval = (*cfg.real_rename) (oldpath, newpath); /* real rename() sets errno. */
	  }
	else /* if absolute_newpath isn't a symlink */
	  {
	     /* (See below for information on this code.) */
	     
	     if (found_under_dir(absolute_newpath, cfg.home)) /* (a) */
	       error = graft_file(cfg.absolute_trash_can, absolute_newpath, cfg.home, &cfg);
	     else /* (b) */
	       error = graft_file(cfg.absolute_trash_system_root, absolute_newpath, NULL, &cfg);
	     
	     if (error) /* graft_file() failed. */
	       {
#ifdef DEBUG
		  fprintf(stderr, "graft_file() failed, invoking rename_handle_error().\n");
#endif
		  retval = rename_handle_error(oldpath, newpath, cfg.real_rename,
					       cfg.in_case_of_failure); /* about errno: see explanation near the previous call to
									 * rename_handle_error(). */
	       }
	     else /* graft_file() succeeded, we just need to perform the "real" operation: */
	       {
#ifdef DEBUG
		  fprintf(stderr, "graft_file(), called by rename(), succeeded.\n");
#endif
		  retval = (*cfg.real_rename) (oldpath, newpath); /* real rename() setting errno. */
	       }
	  }
	
	break;
     }
   
   
   /* ++++++++++++++ */
   
   /* About the code in case BE_SAVED: (a) a file under the user's home dir and (b) a file outside of the user's home 
    dir with global_protection set. We then pass our arguments to the
    "real" rename(), now that the file originally at newpath is safely
    stored in the user's trash can. If graft_file() failed (and a copy of
    the file ISN'T safely stored in the user's trash can), we invoke
    rename_handle_error() which will act based on the value the user chose
    for in_case_of_failure. */
   
   /* Free memory before quitting: */
   
   free(absolute_newpath);
   
   libtrash_fini(&cfg);
   return retval; /* By now, errno has been set to a meaningful value in one of the cases above. */
}

/* This wrapper is less "faithful" done the other ones. Rather than invoking
 * the real GNU libc renameat(), it invokes our "doctored" rename() which, in
 * turn, might call the actual GNU libc rename(). Ie, the real renameat() is 
 * never used. See important note in the  CHANGE.LOG about our wrapping of the
 * *at() functions. */

#ifdef AT_FUNCTIONS

int renameat(int olddirfd, const char *arg_oldpathname, 
	     int newdirfd, const char *arg_newpathnam);

int renameat(int olddirfd, const char *arg_oldpathname, 
	     int newdirfd, const char *arg_newpathname)
{
   int retval = 0;
   
   char *real_oldpath = make_absolute_path_from_dirfd_relpath(olddirfd, arg_oldpathname);
   char *real_newpath = make_absolute_path_from_dirfd_relpath(newdirfd, arg_newpathname);
   
   if (real_oldpath != NULL && 
       real_newpath != NULL)
     {
	/* invoke our own rename(); all the libtrash wrapping madness occurs there, no need to duplicate it here */
	retval = rename(real_oldpath, real_newpath);
     }
   else
     {
	retval = -1; /* XXX this exit should also be handled according to the rule specified by in_case_of_failure; for the time being we just "fail" */
     }
   
   if (real_oldpath != arg_oldpathname)
     {
	free(real_oldpath); /* free mem allocated by make_absolute_path_from_dirfd_relpath() above */
     }
   
   if (real_newpath != arg_newpathname)
     {
	free(real_newpath); /* free mem allocated by make_absolute_path_from_dirfd_relpath() above */
     }
   
   return retval;
}

#endif

/* ------------------------------------------------------------------------------------ */

/* This function is called by rename() in case an error is spotted. It does what the user 
 * told us to do in these situations (through the variable in_case_of_failure), and rename() just
 * returns the value passed by rename_trash_error():
 */


static int rename_handle_error(const char *oldpath, const char *newpath, 
			       int (*real_rename) (const char*, const char*), int in_case_of_failure)
{
   if (in_case_of_failure == ALLOW_DESTRUCTION)
     return (*real_rename) (oldpath, newpath); /* real rename() sets errno */
   else /* if (in_case_of_failure == PROTECT) */
     {
	errno = 0;
	return -1;
     }
}


