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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE /* for access to canonicalize_file_name() inside stdlib.h */
#define __USE_GNU 1 /* for access to get_current_dir_name() inside unistd.h
		     * for access to dlvsym and RTLD_NEXT */
#define __USE_ATFILE 1 /* for access to AT_REMOVEDIR/AT_FDCWD macros inside fcntl.h */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <regex.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "trash.h"

/* This file contains a series of helper functions, which the wrappers use. Their
 * definition is preceded by a short explanation of what they do. */

/* --------------------------------------------- */

/* There isn't any need to export these functions (they are mere "helper helper functions" :-) ): */

static char** read_config_from_file(const char *conf_file, int number_of_keys, config *cfg, ...);

static char *readline(FILE* stream, int *errors);

static int reformulate_new_path(char **new_path, char **first_null);

static int move(const char *old_path, const char *new_path, config *cfg);

static int is_an_exception(const char *path, const char *exceptions);

static int is_empty_file(const char *path);

static int file_is_too_large(const char *path, unsigned long long preserve_files_larger_than_limit);

static int matches_re(const char *path, const char *regexp);

/* Definition of helper functions: */

/* ------------------------------------------------------------------------ */

	static inline int
ilog10 (int n)
{
	int result = 0;

	while (n /= 10)
		result++;

	return result;
}

/* ------------------------------------------------------------------------ */

/* This function returns 1 if the given path points to a file or directory under one
 * of the directories listed in the semi-colon separated dir_list, 0 otherwise. We don't
 * use strtok_r() because doing so would involve creating both a copy of dir_list and an
 * empty buffer of the same size in memory, and, quite frankly, checking for an error code
 * after each call to found_under_dir() in libtrash would make the code very ugly. (That's what
 * would be required if this function depended on malloc()ing memory, an operation which might
 * fail.) It is meant to be a "primitive" which can be used without having to worry about internal
 * failure.
 */

int found_under_dir(const char *abs_path, const char *dir_list)
{
	const char *beg_name = NULL, *end_name = NULL;
	const char *semi_colon = NULL;

	beg_name = dir_list;

	/* Special case: dir_list is NULL, meaning that the user didn't provide us with a list of directories.
	 * Just return 0: */

	if (dir_list == NULL)
		return 0;

	while (*beg_name != '\0')
	{
		semi_colon = strchr(beg_name, ';');

		if (semi_colon)
		{
			if (semi_colon > dir_list && /* we don't want to accidentally access forbidden mem areas */
					*(semi_colon - 1) == '/') /* dir name has a trailing slash, ignore it */
				end_name = semi_colon - 1;
			else /* the first char after the dir name is really the semi-colon */
				end_name = semi_colon;
		}
		else /* if there aren't any more semi-colons, point it to the '\0' at the end of the string or the last '/': */
		{
			if (dir_list[strlen(dir_list) - 1] == '/')
				end_name = dir_list + strlen(dir_list) - 1; /* '/' is the char after the last dir name */
			else
				end_name = dir_list + strlen(dir_list); /* '\0' is the last char after the last dir name */
		}

		if (!strncmp(abs_path, beg_name, end_name - beg_name) && *(abs_path + (end_name - beg_name)) == '/')
			return 1;

		if (semi_colon)
			beg_name = semi_colon + 1;
		else /* if there aren't any more semi-colons, point beg_name to the '\0' at the end of dir_list: */
			beg_name = dir_list + strlen(dir_list);
	}

	/* If we get to this point, return signalling that abs_path isn't located under any of the dirs in dir_list: */

	return 0;

}

/* ---------------------------------------------------------------------- */

/* This function is very similar to found_under_dir: it simply checks whether path starts with one
 * of the file paths listed in exceptions. If it does, this function returns 1; if it doesn't, it
 * returns 0. */

static int is_an_exception(const char *path, const char *exceptions)
{

	const char *beg_name = NULL, *end_name = NULL;
	const char *semi_colon = NULL;

	beg_name = exceptions;

	/* Special case: exceptions is NULL, meaning that the user didn't provide us with such a list of directories.
	 * Just return 0: */

	if (exceptions == NULL)
		return 0;

	while (*beg_name != '\0')
	{
		semi_colon = strchr(beg_name, ';');

		if (semi_colon)
			end_name = semi_colon;
		else /* if there aren't any more semi-colons, point it to the '\0' at the end of the string: */
			end_name = exceptions + strlen(exceptions); /* '\0' is the last char after the last dir name */

		if (!strncmp(path, beg_name, end_name - beg_name)) /* path is equal to (or at least begins with) a path found */
			return 1;                                        /* in EXCEPTIONS */

		if (semi_colon)
			beg_name = semi_colon + 1;
		else /* if there aren't any more semi-colons, point beg_name to the '\0' at the end of cfg->exceptions: */
			beg_name = exceptions + strlen(exceptions);
	}

	/* If we get to this point, return signalling that path isn't one of the exceptions: */

	return 0;
}

/* --------------------------------------------- */

/* This function tests the existence and permissions of the dir's pathname; if it exists and has
 * the right permissions, it returns 1. If it exists but has the wrong
 * permissions and we can't correct them, or if pathname points to a file,
 * it returns 0. If nothing exists at pathname, it tries to create a dir
 * with full permissions by that name. If that succeeds, it
 * returns 1, otherwise 0.
 *
 * Additionally, if it fails due to a name collision (i.e., a file or a dir with that pathname and
 * with wrong - and incorrectable - permissions exists) and the pointer name_collision isn't NULL,
 * it stores 1 where that pointer points to.
 *
 * It is used to recreate the necessary tree structure in the user's trash can.
 */

int dir_ok(const char *pathname, int *name_collision)
{
	int error = 0;

	struct stat dir_stat;

	/* Run stat() on the given pathname: */

	error = stat(pathname, &dir_stat);

	/* Doesn't a file or dir with such a name exist? */

	if (error && (errno == ENOENT))
	{
		/* Try to create it: */

		error = mkdir(pathname, S_IWUSR | S_IRUSR | S_IXUSR);

		/* Did it fail? */

		if (error)
		{
			if (name_collision)
				*name_collision = 0;
			return 0;
		}
		else
			return 1;
	}
	else if (error) /* Some other, more obscure, error occurred... just bail out: */
	{
		if (name_collision)
			*name_collision = 0;
		return 0;
	}

	/* By now, we are sure that something with this name exists, but is it a directory? */

	if (!S_ISDIR(dir_stat.st_mode))
	{
		/* If the pointer name_collision isn't NULL, this means we should sign that we are returning
		 * 0 because of a name collision: */

		if(name_collision)
			*name_collision = 1;
		return 0;
	}

	/* By now, we can be sure that pathname exists and is a directory - but do we have the necessary permissions?
	 * (Note: We don't test for R_OK because libtrash itself doesn't require read-permission, but when we invoke
	 * mkdir()/chmod() we set it to allow browsing of the trash can.)
	 */

	if (!access(pathname, W_OK | X_OK))
		return 1;
	else
	{

		/* Try to change the permissions of that dir: */

		error = chmod(pathname, S_IWUSR | S_IRUSR | S_IXUSR);

		if (error)
		{
			/*
			 * If the pointer name_collision isn't NULL, this means we should sign that we are returning
			 * 0 because of a name collision: */

			if(name_collision)
				*name_collision = 1;
			return 0;
		}
		else
			return 1;
	}

}

/* -------------------------------------------------------------- */

/* This function reproduces the directory structure described in branch under tree,
 * ignoring the part of branch contained in what_to_cut. It then moves the file at
 * the end of branch to the end of the newly-created directory hierarchy under tree.
 * If what_to_cut is NULL, we do not "skip over" any part of the path contained in
 * branch.
 *
 * E.g.
 *
 * graft_file("/new_dir", "/a/b/c/file.txt", "/a", &cfg);
 *
 * results in rename()ing file "/a/b/c/file.txt" to "/new_dir/b/c/file.txt".
 *
 * Or:
 *
 * graft_file("/new_dir", "/a/b/c/file.txt", NULL, &cfg);
 *
 * moves file "/a/b/c/file.txt" to "/new_dir/a/b/c/file.txt".
 *
 * In case of success, we return 0; otherwise, we return either -2 (if it
 * proves impossible to effectively move old_path to the trash can - this
 * meaning that either (a) old_path is on a RO filesystem, or (b) the user
 * lacks either write-permisssion to the directory which holds old_path (if
 * the real rename() is used) or (c) the user lacks read-permission to the
 * file old_path itself (if we resort to "manually" moving it), or -1
 * (insufficient memory or other serious error). This distinction is used
 * by the wrapper around unlink() to decide whether to return with errno
 * set to 0 or to EACCES/EPERM/EROFS. */

int graft_file(const char *new_top_dir, const char *old_path, const char *what_to_cut, config *cfg)
{
	/* We assume that tree exists and we have write- and search permissions to it, because it is
	 * either absolute_trash_can or absolute_trash_system_root, and their existence
	 * and their permissions have been checked by _init().
	 *
	 * We assume that the file pointed to by branch resides under what_to_cut (if what_to_cut
	 * is NULL, it represents the filesystems root dir, /), because the arguments we were given
	 * have been chosen to guarantee that this is so.
	 */

	char *new_path = NULL;

	char *ptr = NULL;

	int error = 0, success = 0, retval = 0;

	int name_collision = 0;

	const char *branch = old_path;

	const char *tree = new_top_dir;

	/* First of all: "cut" what_to_cut from branch. For the above-mentioned reasons, we don't
	 * perform any checks, except for testing whether what_to_cut is NULL:
	 */

	if (what_to_cut)
		branch += strlen(what_to_cut);

	/* Now branch points to the relative path to the file which we must reproduce in its entirety
	 * under tree. Let's form new_path, which is the concatenation of tree and (the new)
	 * branch:
	 */

	new_path = malloc(strlen(tree) + strlen(branch) + 1);

	if (!new_path)
	{
#ifdef DEBUG
		fprintf(stderr, "Unable to allocate sufficient memory.\nlibtrash turned off.\n");
#endif

		return -1;
	}

	strcpy(new_path, tree);
	strcat(new_path, branch);

	/* We proceed in the following way:
	 *
	 * (0) point ptr to the character which follows the end of tree in new_path (first char after
	 * the slash);
	 *
	 * (a) we search for the next slash after ptr, mark it with ptr and overwrite
	 * it with '\0'; if no slash is found, we go to (f);
	 *
	 * (b) we feed new_path to dir_ok(): this will test if this directory (whose name is delimited by the
	 * '\0' at ptr) already exists (and has the right permissions), and, if it doesn't, dir_ok() will attempt to
	 * correct the situation;
	 *
	 * (c) if dir_ok() succeeds, we put the slash back in place (where ptr points to) , move ptr 1 byte
	 * forward and go back to (a), in order to check/create the next subdir;
	 *
	 * (d) if dir_ok() failed for a reason other than a name collision, we fail and return -1;
	 *
	 * (e) if dir_ok() failed due to a name collision, we invoke reformulate_new_path(), which
	 *
	 *    i)   points new_path to a viable file path,
	 *    ii)  puts the slash back in place and
	 *    iii) rewinds ptr so that it points to the beginning of the new, suggested directory name (which
	 *         will be created by dir_ok()), instead of pointing at the slash that follows it.
	 *
	 * We then go back to (a).
	 *
	 * (f) we are done, we just need to rename() the file (whose name is stored at old_path) to new_path.
	 */

	ptr = new_path + strlen(tree) + 1;

	while ( (ptr = strchr(ptr, '/')) )
	{
		*ptr = '\0';

		success = dir_ok(new_path, &name_collision);

		if (!success)
		{
			if (!name_collision)
			{
#ifdef DEBUG
				fprintf(stderr, "graft_file() is returning -1 because the call to dir_ok()"
						" failed for a reason other than a name collision.\n"
						"new_path: %s\n", new_path);
#endif

				free(new_path);

				return -1;
			}

			else /* if we failed due to a name collision */
			{
#ifdef DEBUG
				fprintf(stderr, "dir_ok() failed due to a name collision. Invoking reformulate_new_path.\n"
						"new_path: %s\n", new_path);
#endif

				error = reformulate_new_path(&new_path, &ptr);

				if (error)
				{
#ifdef DEBUG
					fprintf(stderr, "graft_file() returning -1 because reformulate_new_path() failed.");
#endif
					free(new_path);

					return -1;
				}
				else /* if reformulate_new_path() succeeded */
					continue;
			}

		}

		*ptr = '/';
		ptr++;
	}

	/* At this point, we are almost ready; we must only see if there is no collision with the file name
	 * itself (i.e., does /a/b/c/file.txt already exist?): */

	if (!access(new_path, F_OK)) /* file already exists */
	{
		error = reformulate_new_path(&new_path, NULL);

		if (error)
		{
#ifdef DEBUG
			fprintf(stderr, "reformulate_new_path() failed, graft_file() returning -1.\n");
#endif
			free(new_path);

			return -1;
		}
	}

	/* The only thing left to do is to rename() the file: */

	retval = (*cfg->real_rename) (old_path, new_path);

#ifdef DEBUG
	fprintf(stderr , "rename() invoked inside graft_file(). Arguments passed: old_path: |%s|; new_path: |%s|\n",
			old_path, new_path);
#endif

	/* If the call to rename() failed because old_path points to a file
	 * on a partition/filesystem other than the one which houses the user's
	 * home directory (and, most importantly, her trash can), then we just
	 * invoke the move() function, which "manually" moves each byte from one
	 * location to the other (in the future, a faster - and perhaps more
	 * intelligent :) - method will be used for this purpose). If rename()
	 * failed for any other reason, we leave retval set to -1 and just pass
	 * that value to the caller, thereby signalling that we failed to "save" a
	 * copy of this file: */

	if (retval && errno == EXDEV)
	{

#ifdef DEBUG
		fprintf(stderr, "%s and %s point to different filesystems, move()ing the file \"manually\".\n",
				old_path, new_path);
#endif

		retval = move(old_path, new_path, cfg); /* if move() succeeds, it return 0; otherwise, it returns either -2
							   (if it failed due to the
							   inability to write) or -1 (any
							   other reason).*/
	}
	else if (retval &&
			(errno == EACCES ||
			 errno == EPERM  ||
			 errno == EROFS) ) /* the real rename() failed due to the inability to write; returning -2: */
		retval = -2;

	/* And free() the memory: */

	free(new_path);

	/* Return: */

#ifdef DEBUG
	fprintf(stderr, "graft_file() returning %d.\n", retval);
#endif

	return retval; /* we return
			*  0 - if either the real rename() or move() succeeded;
			* -1 - if either the real rename() or move() failed for a reason other than insufficient permissions;
			* -2 - if either the real rename() or mov() failed due to the inability to write.
			* The only wrapper which distinguishes between -1 and -2
			* is unlink(), since it needs to set errno itself. */
}

/* ------------------------------------------------------------------------------- */

/* reformulate_new_path()
 *
 * What it does: called if there is a name collision, this function substitutes the "offending"
 * dir or filename with one ressembling the desired one but which doesn't already exist. The new file/dir
 * name has the form "original"[n], where n is the lowest natural number which avoids a collision.
 *
 * If the second argument is NULL, we rewrite the file's name itself. If it isn't, it is assumed to
 * be a pointer to a pointer to the null character which ends the "offending" dir name in the string
 * pointed to by the pointer to which the first argument points. (I think I got this right! :-))
 *
 * In case of an allocation error, we return -1; otherwise we return 0.
 */

int reformulate_new_path(char **new_path, char **first_null)
{

	char *possible_path = NULL;

	struct stat stat_var;

	int error = 0;

	int n = 1;

	int digits_number = DEF_DIGITS;

	int left = 0;
	int right = -1;

	/* This memory will be free()d either after having stored a new path in *new_path or
	 * inside the calling function (graft_files()), depending on whether first_null is
	 * NULL or not.
	 */

	possible_path = malloc(strlen(*new_path) + 1 + digits_number + 1 + 1); /* two extra bytes for the brackets */

	if (!possible_path)
	{
#ifdef DEBUG
		fprintf(stderr, "Unable to allocate sufficient memory (reformulate_new_path()).\n");
#endif
		return -1;
	}

	errno = 0;

	while (1)
	{
		sprintf(possible_path, "%s[%d]", *new_path, n);

		error = stat(possible_path, &stat_var);

		if (error && errno == ENOENT) /* Jacek made this into a logarithmic search */
		{
			if (left == n - 1)
				break;
			right = n;
			n = (n + left) / 2;
		}
		else
		{
			left = n;
			if (right == -1)
				n *= 2;
			else if (right > n + 1)
				n = (n + right) / 2;
			else {
				n++;
				right = -1;
			}
		}

		if (first_null != NULL && /* if we have been asked to invent a new a _dir_ name */
				!error  &&            /* and if something with this name (possible_path) already exists */
				S_ISDIR(stat_var.st_mode) && /* and this something is a directory */
				!access(possible_path, W_OK | X_OK) ) /* to which you have write- and search-permission */
			break;                           /* then stop making up new names because this one serves us. */

		/* Do we need more memory? */

		if (ilog10(n) >= digits_number)
		{
			char *tmp = NULL;

			digits_number *= REALLOC_FACTOR;

			tmp = realloc(possible_path, strlen(*new_path) + 1 + digits_number + 1 + 1); /* two bytes for brackets */

			if (!tmp)
			{
#ifdef DEBUG
				fprintf(stderr, "Unable to allocate sufficient memory (reformulate_new_path()).\n");
#endif
				free(possible_path);
				return -1;
			}
			possible_path = tmp;
		}
	}

	/* By now, possible_path holds a "good" replacement for the part of *new_path which goes up to
	 * the first '\0', independently of that being the "entire" path or just a part of it. (I mean:
	 * if the problem was the name of a directory, we now have an alternative dir name; if the problem
	 * was the name of the file itself, we now have a complete new path for that file.)
	 *
	 * So, if we were passed NULL as the second argument (which means that we already have all that
	 * we need in possible_path), we just:
	 */

	if (!first_null)
	{
		free(*new_path);
		*new_path = possible_path;

		/* [The memory allocated at possible_path will be free()d in graft_files().] */

		return 0;
	}

	/* If we have in possible_path an alternative dir name to the one held in *new_path up to *first_null,
	 * we must now concatenate possible_path with the part of the original path which follows *first_null:
	   */
	else
	{
		char *tmp  = NULL;
		char *tmp2 = NULL;

		/* Before doing so, we must put the slash back in its place:*/

		**first_null = '/';

		tmp = malloc(strlen(possible_path) + strlen(*first_null) + 1);

		if (!tmp)
		{
#ifdef DEBUG
			fprintf(stderr, "Unable to allocate sufficient memory (reformulate_new_path()).\n");
#endif
			free(possible_path);
			return -1;
		}

		strcpy(tmp, possible_path);
		strcat(tmp, *first_null);

		/* Now we have the entire, "corrected" path at tmp. */

		/* Update pointer at *first_null:
		 *
		 * First, we make it point to its original location, the slash
		 * which followed the "offending" dir name: */

		*first_null = tmp + strlen(possible_path);

		/* [This is a good time to free this memory:] */

		free(possible_path);

		/* We now wish to point the pointer at *first_null at the character which follows the last
		 * slash _before_ the one currently pointed to by *first_null.
		 * The slash currently pointed to by *first_null  marks the end
		 * of the suggested new dir name, and the previous one marks its
		 * beginning - precisely what we are interested in, so that inside graft_file()
		 * dir_ok() is invoked to create this new dir:
		 */

		/* Temporarily remove the slash which terminates the alternative name: */

		**first_null = '\0';

		/* With that slash temporarily removed, find the one which precedes it: */

		tmp2 = strrchr(tmp, '/');

		/* I think we can be _sure_ that the string to which the pointer to which our first
		 * argument points is an absolute path, whose top dir is absolute_trash_can, whose
		 * existence has been checked. So, we should also be sure that the dir we are being
		 * asked to invent a name for is under absolute_trash_can. This would mean that the
		 * slash which ends its name _can't_ the last one in this path, and that there isn't
		 * any reason for tmp2 to be NULL.
		 *
		 * But, if I am missing something, we won't be taking any chances and just abort:
		 */

		if (!tmp2)
		{
#ifdef DEBUG
			fprintf(stderr, "String manipulation error: unable to find previous slash in path, "
					"function reformulate_new_path.\n");
#endif
			free(tmp);
			return -1;
		}

		/* Put the last slash back in place: */

		**first_null = '/';

		/* Now just update *first_null: */

		*first_null = tmp2 + 1;

		/* *first_null now points to the new dir name's first
		 * character, so that, when back in graft_files(), the expression
		 *
		 * ptr = strchr(ptr, '/') (in the test of the while loop)
		 *
		 * makes ptr point to the last slash, i.e., the one which marks the end of
		 * the new dir name, which will be created by dir_ok().
		 */

		/* Free memory: */

		free(*new_path);

		/* Update the pointer to which the first argument points, so that it now points to an alternative
		 * path:
		 */

		/* [This memory will be free()d in graft_file().] */

		*new_path = tmp;

		return 0;
	}

}

/* --------------------------------------------------------------------------- */

/* hidden_file().
 *
 * This function returns 1 if the specified file is either a hidden file (i.e., its name
 * begins with a dot) or resides under a hidden directory (i.e., under a directory whose
 * name begins with a dot). It returns 0 otherwise.
 *
 * Because absolute_path has already been "canonicalized", there's no need to worry about symlinks such as ".." and ".".
 *
 */

int hidden_file(const char *absolute_path)
{
	if (strstr(absolute_path, "/."))
		return 1;
	else
		return 0;
}

/* ---------------------------------------------------------------------------- */

/* What this function does: being passed a (either relative or absolute)
 * path, it performs the following actions:
 *
 * if should_follow_final_symlink is non-zero, path exists and its last component
 * is a symlink, then this function: simply invokes canonicalize_file_name().
 * That is, it resolves the symlink at 'path' and returns the absolute path of
 * whatever it points to.
 *
 * else (meaning that either (i) should_follow_final_symlink is zero, (ii) path
 * doesn't exist or (iii) path exists but is NOT a symlink):
 *
 * 1 - isolates the filename in path from the rest of the path which precedes it
 * (e.g., extract "file" from "/tmp/dir/file";
 * 2 - run canonicalize_file_name() on the rest of the path;
 * 3 - return the concatenation of the return value of canonicalize_file_name()
 * with the filename in case of success, NULL otherwise.
 *
 * The reason why it is useful and we don't always call canonicalize_file_name() directly instead
 * is because, if the last component (the filename) is a symlink, canon...() resolves it.
 * This makes it useless in the unlink() and rename() wrappers, because those functions
 * act upon the symlinks themselves, not the files they point at. But they still need to
 * resolve any other symlinks in the path, so we can't just use the absolute "unresolved"
 * path.
 *
 * Most importantly, canonicalize_file_name() always fails on paths to inexistent
 * files. This function provides a convenient wrapper around it (which I use in
 * some non-libtrash code).
 *
 * Return NULL on case of error.
 */

char* build_absolute_path(const char *path, int should_follow_final_symlink)
{
	char *absolute_path = NULL;

	char *dirname = NULL, *abs_dirname = NULL;

	char *slash = strrchr(path, '/');

	/* Check if we should simply call canonicalize_file_name(): */

	if (should_follow_final_symlink)
	{
		struct stat st;

		int error = stat(path, &st);

		if (!error && S_ISLNK(st.st_mode))
		{
			return canonicalize_file_name(path);
		}
	}

	/* 1- Separate the directory name from the filename: */

	/* If path is just a filename, we use the current working directory as a basis: */

	if (!slash)
		dirname = get_current_dir_name();
	else /* if path contains a directory name: */
	{
		if (slash == path) /* path has the form "/fsdds.txt", and dirname is a single "/" */
		{
			dirname = malloc(2);

			if (dirname)
				strcpy(dirname, "/");
		}
		else /* path has the form "/fds/fds.txt", and dirname is "/fds" */
		{
			dirname = malloc(slash - path + 1); /* (1) malloc() instead of alloca() so that dirname is always
							       free()d, independently of that memory having been allocated inside
							       get_current_dir_name() or by us; (2) no byte for trailing slash */

			if (dirname)
			{
				strncpy(dirname, path, slash - path);
				dirname[slash - path] = '\0';
			}
		}
	}

	/* In any case, if dirname isn't NULL it now holds the path to the dir which will contain the file we
	   will be creating. We first canonicalize it, and then suffix it with the filename found in path: */

	/* 2- canonicalize dirname: */

	if (dirname)
	{
		abs_dirname = canonicalize_file_name(dirname);
		free(dirname); /* get_current_dir_name() malloc()s the memory it returns a pointer to, just us we do */

		if (abs_dirname)
		{
			/* 3- If we have the absolute path to the bottom level dir which will hold this file,
			   we compose an absolute_path composed of abs_dirname + '/' + filename: */

			absolute_path = malloc(strlen(abs_dirname) + 1 +
					(slash ? strlen(slash + 1) : strlen(path)) /* space for filename */
					+ 1);
			if (absolute_path)
			{
				strcpy(absolute_path, abs_dirname);

				if (strlen(abs_dirname) > 1)  /* only append an extra slash if abs_dirname is something other than a single slash... */
					strcat(absolute_path, "/");

				if (slash)
					strcat(absolute_path, slash + 1);
				else /* (!slash) */
					strcat(absolute_path, path);
			}

			free(abs_dirname);
		}
	}

	/* Just return absolute_path; if we failed along the way, it will be set to NULL, which will
	   be interpreted by the caller as an error code: */

	return absolute_path;
}

/* ----------------------------------------------------------------------- */
/* What this function does: if pathname ends in one of the extensions listed in ignore_extensions,
 * it returns 1; otherwise, it returns 0. We don't use strtok() because that would require a copy
 * of the ignore_extensions buffer and creating one would expose us to nasty exception handling
 * problems:
 */

int ends_in_ignored_extension(const char *filename, config *cfg)
{
	const char *beg_extension = NULL, *end_extension = NULL;
	const char *semi_colon = NULL, *last_slash = NULL, *last_dot = NULL;
	const char *filename_ext = NULL;

	/* Point beg_extension to the beginning of the first extension listed in ignore_extensions: */

	beg_extension = cfg->ignore_extensions;

	/* Point filename_ext to the beginning of the extension of filename, if any (in this case return 0): */

	last_dot = strrchr(filename, '.');

	last_slash = strrchr(filename, '/');

	/* If the filename contains a dot and a slash and the last slash in it
	 * comes _after_ the last dot then that dot doesn't mark the beginning
	 * of an extension and we return 0, because this filename has no
	 * extension at all: */

	if (!last_dot ||
			(last_slash && last_dot < last_slash) ||
			*(last_dot + 1) == '\0')
		return 0;

	filename_ext = last_dot + 1; /* this pointer points to the extension in the filename we are testing */

	while (*beg_extension != '\0')
	{
		semi_colon = strchr(beg_extension, ';');

		if (semi_colon)
			end_extension = semi_colon;
		else /* if there aren't any more semi-colons, point end_extension to the '\0' at the end of cfg->ignore_extensions: */
			end_extension = cfg->ignore_extensions + strlen(cfg->ignore_extensions);

		/* If the (recognized) extension beg_extension now points to is the same as the one at the end of filename,
		 * return 1: */

		if (!strncmp(beg_extension, filename_ext, end_extension - beg_extension) &&
				filename_ext[end_extension - beg_extension] == '\0')
			return 1;

		if (semi_colon)
			beg_extension = end_extension + 1;
		else /* if there aren't any more semi-colons, point beg_extension to the '\0' at the end of ignore_extensions: */
			beg_extension = end_extension;
	}

	/* If we get to this point, return signalling that filename doesn't end in any of the extensions listed in
	 * ignore_extensions: */

	return 0;

}

/* -------------------------------------------------------------------- */

/* What this function does: passed an integer (number_of_keys) and
 * number_of_keys pointers to char, it either opens a file called filepath in the user's
 * home dir (if filepath is a relative path) or a system-wide configuration
 * file (if filepath is an absolute_path) and tries to extract the values
 * matching those keys from lines which have the form of (key) = (value)
 *
 * If it succeeds, it returns a pointer to an array of pointers to the read
 * values, which are stored in the same order as the one used in the
 * argument list (i.e., the value matching key1 is the first in the
 * returned array, the value matching key2 is the second, etc...). For each
 * key for which we couldn't read a value, a NULL pointer is stored in the
 * corresponding array slot (i.e., if no value was found for key3,
 * values_array[2] will be NULL). In case a serious error happened (not
 * being able to read a value for one of the keys provided DOESN'T qualify:
 * handling such situations is something better left to the caller), a NULL
 * pointer is returned. */

static char** read_config_from_file(const char *filepath, int number_of_keys, config *cfg, ...)
{

	FILE *conf_file = NULL;

	struct passwd *userinfo = NULL;

	char *conf_file_path = NULL;

	char *line = NULL;

	va_list arg_list;

	int i = 0;

	int error = 0;

	/* Each of these arrays will store number_of_keys pointers to the keys and the read values, respectively: */

	const char **keys_array = NULL;

	char **values_array = NULL;

	/* Make sure that we are able to use the real fopen(): */

	if (!cfg->real_fopen)
	{

#ifdef DEBUG
		fprintf(stderr, "real_fopen is not available, bailing out! (read_config_from_file()).\n");
#endif

		return NULL;
	}

	/* Allocate memory for the keys_array (which we will use to store the keys passed as an argument) and
	   the values_array (to which we will return a pointer if we succeed): */

	keys_array   = malloc(number_of_keys * sizeof(char*)); /* this is an "internal" array */

	if (!keys_array)
	{
#ifdef DEBUG
		fprintf(stderr, "Insufficient memory (read_config_from_file()).\n");
#endif

		return NULL;
	}

	values_array = malloc(number_of_keys * sizeof(char*)); /* this is the array we will return in case of success */

	if (!values_array)
	{
#ifdef DEBUG
		fprintf(stderr, "Insufficient memory (read_config_from_file()).\n");
#endif

		/* Free already allocated memory: */

		free(keys_array);

		return NULL;
	}

	/* If both allocations succeeded, "zero" the values_array, so that if we can't find a value for
	 * one of the keys in the keys_array we don't need to do anything at all
	 * because that slot will already be set to NULL (which the caller
	 * interprets as meaning that no value could be found/read for that
	 * key): */

	for (i = 0; i < number_of_keys; i++)
		values_array[i] = NULL;

	/* Read the arguments we were passed and store them in the keys_array:  */

	va_start(arg_list, cfg);

	for (i = 0; i < number_of_keys; i++)
		keys_array[i] = va_arg(arg_list, char*);

	va_end(arg_list);

	/* From this point we must act according to the string in filepath: should we read
	 * a system-wide configuration file or a personal one? If filepath holds an absolute_path,
	 * we read a system-wide configuration file and we don't need to create an absolute path to
	 * that file; if filepath holds a relative path, we must build an absolute path to that
	 * file in the user's home dir: */

	if (filepath[0] == '/') /* holds an absolute path, nothing for us to do (at least for now... :)) */
		conf_file_path = filepath;
	else /* filepath holds a path relative to the user's home dir: */
	{
		userinfo = getpwuid(geteuid());

		/* If it failed, we just return  NULL: */

		if (!userinfo)
		{
#ifdef DEBUG
			fprintf(stderr, "Unable to determine information about the user.\n");
#endif

			/* Free memory: */

			free(keys_array);
			free(values_array);

			return NULL;
		}

		/* We will now compose the absolute path to the user's configuration file... : */

		conf_file_path = malloc(strlen(userinfo->pw_dir) + 1 + strlen(filepath) + 1);

		if (!conf_file_path)
		{
#ifdef DEBUG
			fprintf(stderr, "Memory allocation error inside read_config_from_file().\n");
#endif
			/* Free memory: */

			free(keys_array);
			free(values_array);

			return NULL;
		}

		strcpy(conf_file_path, userinfo->pw_dir);

		strcat(conf_file_path, "/");

		strcat(conf_file_path, filepath);
	}

	/* ... and use it in a call to the real fopen(): */

	conf_file = (*cfg->real_fopen) (conf_file_path, "r");

	/* Did fopen() succeed? */

	if (!conf_file)
	{
#ifdef DEBUG
		fprintf(stderr, "Unable to open config file at %s.\n", conf_file_path);
#endif

		if (conf_file_path != filepath)
			free(conf_file_path); /* If conf_file_path isn't pointing to the first argument, then free memory: */

		/* Free memory: */

		free(keys_array);
		free(values_array);

		return NULL;
	}

	if (conf_file_path != filepath) /* If conf_file_path isn't pointing to the first argument, then free memory: */
		free(conf_file_path);

	/* Read each line of the file; compare the portion before the '=' to each of the items in the keys_array;
	 * if there's a match with the nth key, store the corresponding value in the nth position of the values_array.
	 */

	while ( (line = readline(conf_file, &error)) )
	{
		char *beg_key = NULL , *end_key = NULL, *beg_value = NULL, *end_value = NULL,
		     *ptr = NULL;

		char *equal_sign = NULL;

		/* Now we extract the key from this line and compare it to the ones listed in keys_array: */

		/* We begin by marking the beginning and the end of the key and the value in the string line
		 * with the above declared pointers: */

		equal_sign = strchr(line, '=');

		ptr = line;

		while (isspace(*ptr)) /* skip any leading white space */
			ptr++;

		/* Does this line
		 * (a) start with a '#' or
		 * (b) lack an equal sign (also detects empty lines) or
		 * (c) start with white space followed by an equal sign (i.e., no key available)? */

		if (line[0] == '#' || !equal_sign || ptr == equal_sign)
		{
			/* Free memory (each time it is called, readline allocates a new buffer and returns it): */

			free(line);

			continue;
		}

		/* -> Line seems OK */

		/* Key: */

		beg_key = ptr; /* key begins here */

		ptr = strchr(beg_key, ' ');

		if (ptr && ptr < equal_sign) /* if we found white space before the equal sign, */
			end_key = ptr - 1; /* the key ends here */
		else  /* if we didn't find white space or only found it after the equal sign */
			end_key = equal_sign - 1; /* the key ends here */

		/* Value: */

		ptr = equal_sign + 1; /* skip equal sign */

		while (isspace(*ptr)) /* skip white space */
			ptr++;

		/* ptr either points to the beginning of a value or to the '\0'. */

		if (*ptr == '\0')    /* (If there's nothing after equal_sign, we set beg_value to NULL.) */
			beg_value = NULL;
		else
		{
			beg_value = ptr;

			while (*ptr != '\0' && !isspace(*ptr)) /* move to the first character after the value */
				ptr++;

			end_value = ptr - 1;
		}

		/* Done marking key/value. We now compare the string between beg_key and end_key to those
		   listed in key_array: */

		for (i = 0; i < number_of_keys; i++)
			if (!strncmp(beg_key, keys_array[i], end_key - beg_key + 1))
			{
				char *tmp = NULL;

				if (beg_value == NULL) /* empty value, one byte is enough */
					tmp = malloc(1);
				else
					tmp = malloc(end_value - beg_value + 2); /* (2 is not a typo!) */

				if (!tmp)
				{
#ifdef DEBUG
					fprintf(stderr, "Insufficient memory (read_config_from_file()).\n");
#endif
					/* Free memory: */

					for (i = 0; i < number_of_keys; i++)
						if (values_array[i] != NULL)
							free(values_array[i]);

					free(keys_array);
					free(values_array);

					free(line);

					/* Close config file: */

					fclose(conf_file);

					return NULL;
				}

				if (beg_value == NULL) /* empty value, just store '\0' */
					*tmp = '\0';
				else
				{
					strncpy(tmp, beg_value, end_value - beg_value + 1);
					tmp[end_value - beg_value + 1] = '\0';
				}

				/* Store a pointer to this string in the values_array: */

				values_array[i] = tmp;

				/* Done. */

				break; /* from the for loop, since we have already found a match. */
			}

		/* Whether we filled in a slot of the values_array or not, we now skip to the next line,
		 * not forgetting to free the memory which currently holds the previously read line: */

		free(line);
	}

	/* (No memory is pointed to by line, since read_line() failed (and no memory was malloc()ed inside it).) */

	/* Close file: */

	fclose(conf_file);

	/* Free keys_array, since we don't need that memory any more: */

	free(keys_array);

	/* We exited the loop because readline returned NULL; but was this due to EOF or to an error? */

	if (error == NOMEM || error == FERROR)
	{
#ifdef DEBUG
		if (error == FERROR)
			fprintf(stderr, "Error reading from disk (readline).\n");
		else /* if (error == NOMEM) */
			fprintf(stderr, "Error while reading from disk (readline()).\n");
#endif
		/* Free all memory and return NULL: */

		for (i = 0; i < number_of_keys; i++)
			if (values_array[i] != NULL)
				free(values_array[i]);

		free(values_array);

		return NULL;
	}

	/* No error occurred, return values_array: */

	return values_array;
}

/* ------------------------*/

/* This function reads an entire line from the file pointer stream,
 * and returns a pointer to it (it is stored in malloc()ed memory). In
 * case of error it returns NULL and sets *errors to the appropriate
 * value. It is only used by read_config_from_file():
 */

static char * readline(FILE *stream, int *errors)
{

#ifndef REALLOC_FACTOR
# define REALLOC_FACTOR 2
#endif

#ifndef DEFAULT_BUF_SIZE
# define DEFAULT_BUF_SIZE 100
#endif

	char *line = NULL, *ptr = NULL;

	int ch = 0;

	size_t line_len = DEFAULT_BUF_SIZE;

	/* Set error indicator to zero: */

	*errors = 0;

	line = malloc(line_len + 1);

	if (!line)
	{
		*errors = NOMEM;
		return NULL;
	}

	ptr = line;

	while ((ch = fgetc(stream)) != EOF && ch != '\n')
	{
		*ptr++ = ch;

		if ((unsigned) (ptr - line) == line_len)
		{
			char *tmp = NULL;
			tmp = realloc(line, REALLOC_FACTOR * line_len + 1);

			if (!tmp)
			{
				free(line);
				*errors = NOMEM;
				return NULL;
			}

			line = tmp;

			ptr = line + line_len;

			line_len *= REALLOC_FACTOR;
		}
	}

	*ptr = '\0';

	if (ferror(stream))
	{
		free(line);
		*errors = FERROR;
		return NULL;
	}

	if (strlen(line) == 0 && feof(stream))
	{
		free(line);
		return NULL;
	}

	return line;
}

/* ----------------------------------------------------------------------------- */

/* This function tries to read the configuration values from a user-specific file called
 * PERSONAL_CONF_FILE in the user's home dir. If that fails, it leaves the
 * compile-time defaults unchanged. */

/* ---------------------------------------------- */

/* These macros are used by the function get_config_from_file(): */

#define NUMBER_OF_CONFIG_OPTIONS 23 /* number of options listed below in the call to read_config_from_file(). */

/* ---------------------------- */

#define SET_INTEGER_FREE_MEMORY(var,str,str1,str2,int1,int2,intdef)	\
{									\
	if (!str)							\
	var = intdef;							\
	else if (!strcmp(str, str1))					\
	var = int1;							\
	else if (!strcmp(str, str2))					\
	var = int2;							\
	else								\
	var = intdef;							\
	if (str)							\
	free(str);							\
}

/* ------------------------------------------------ */

void get_config_from_file(config *cfg)
{

	char **config_values = NULL;

	/* We first try to read the user specific file: */

	/* Call read_config_from_file() with the approriate arguments: */

	config_values = read_config_from_file(PERSONAL_CONF_FILE, NUMBER_OF_CONFIG_OPTIONS, cfg,
			"TRASH_CAN",
			"IN_CASE_OF_FAILURE",
			"SHOULD_WARN",
			"IGNORE_HIDDEN",
			"IGNORE_EDITOR_BACKUP",
			"PROTECT_TRASH",
			"GLOBAL_PROTECTION",
			"TRASH_SYSTEM_ROOT",
			"TEMPORARY_DIRS",
			"USER_TEMPORARY_DIRS",
			"UNREMOVABLE_DIRS",
			"IGNORE_EXTENSIONS",
			"INTERCEPT_UNLINK",
			"INTERCEPT_RENAME",
			"INTERCEPT_FOPEN",
			"INTERCEPT_FREOPEN",
			"INTERCEPT_OPEN",
			"LIBTRASH_CONFIG_FILE_UNREMOVABLE",
			"REMOVABLE_MEDIA_MOUNT_POINTS",
			"IGNORE_EDITOR_TEMPORARY",
			"EXCEPTIONS",
			"IGNORE_RE",
			"PRESERVE_FILES_LARGER_THAN");

	/* Did read_config_from_file() fail? If it did, we quit and leave the compile-time defaults unchanged: */

	if (!config_values)
		return;

	/* If we managed to read the configuration file in the user's home directory,
	 * we now proceed to set the configuration variables to the values read_config_from_file() returned to us: */

	/* Configuration variables which are integers used as "flags" are set by the
	 * SET_INTEGER_FREE_MEMORY(var, str,str1,str2,int1,int2,intdef) macro, which translates the string str
	 * to the matching macro-defined (integer) value and sets the integer var to that value:
	 *
	 * - if the string str is equal to str1, then var is set to int1;
	 * - if the string str is equal to str2, then var is set to int2;
	 * - if the string str is neither equal to str1 nor equal to str2, then var is set to intdef.
	 *
	 * Memory used by strings which are translated into an integer value is then free()d. */

	/* Name of trash can (must be a string with more than 0 characters): */

	if (config_values[0]) /* memory was malloc()ed */
	{
		if (strlen(config_values[0]) > 0) /* something we can use, will be free()d at the end of _init(): */
			cfg->relative_trash_can = config_values[0];
		else /* empty string, just free() the memory right away and use the compile-time default: */
			free(config_values[0]);
	}

	/* What to do in case of failure: */

	SET_INTEGER_FREE_MEMORY(cfg->in_case_of_failure, config_values[1], "ALLOW_DESTRUCTION", "PROTECT",
			ALLOW_DESTRUCTION, PROTECT, IN_CASE_OF_FAILURE);

	/* Whether we should inform the user that libtrash is disabled: */

	SET_INTEGER_FREE_MEMORY(cfg->should_warn, config_values[2], "YES", "NO",
			YES, NO, SHOULD_WARN);

	/* Whether to ignore hidden files (or files under hidden dirs): */

	SET_INTEGER_FREE_MEMORY(cfg->ignore_hidden, config_values[3], "YES", "NO",
			YES, NO, IGNORE_HIDDEN);

	/* Whether to ignore back-up files used by text editors: */

	SET_INTEGER_FREE_MEMORY(cfg->ignore_editor_backup, config_values[4], "YES", "NO",
			YES, NO, IGNORE_EDITOR_BACKUP);

	/* What to do when the loss of a file inside the user's trash can may result: */

	SET_INTEGER_FREE_MEMORY(cfg->protect_trash, config_values[5], "YES", "NO",
			YES, NO, PROTECT_TRASH);

	/* What to do when the loss of a file outside of the user's home dir may result: */

	SET_INTEGER_FREE_MEMORY(cfg->global_protection, config_values[6], "YES", "NO",
			YES, NO, GLOBAL_PROTECTION);

	/* Which name to use for the dir under which we store "alien" files if global_protection is set: */

	if (config_values[7]) /* points to malloc()ed memory */
	{
		if (cfg->global_protection && strlen(config_values[7]) > 0) /* global_protection is set and this is a "valid" string */
			cfg->relative_trash_system_root = config_values[7];       /* will be free() near the end of _init() */
		else /* if malloc()ed memory is pointed to by config_values[7] but we won't be using it, free() it right away and */
			free(config_values[7]); /* use the compile-time default */
	}

	/* No test for strlen() > 0 for these three because the user can legitimately set any of these values to the empty
	   string (meaning that she doesn't wish to enable these features): */

	if (config_values[8])
		cfg->temporary_dirs = config_values[8];

	if (config_values[9])
		cfg->user_temporary_dirs = config_values[9];

	if (config_values[10])
		cfg->unremovable_dirs = config_values[10];

	if (config_values[11])
		cfg->ignore_extensions = config_values[11];

	/* (All of the memory pointed to by these three pointers will be free()d in _fini().)  */

	/* Should we (dis)enable any function specifically? */

	SET_INTEGER_FREE_MEMORY(cfg->intercept_unlink, config_values[12], "YES", "NO",
			YES, NO, INTERCEPT_UNLINK);

	SET_INTEGER_FREE_MEMORY(cfg->intercept_rename, config_values[13], "YES", "NO",
			YES, NO, INTERCEPT_RENAME);

	SET_INTEGER_FREE_MEMORY(cfg->intercept_fopen, config_values[14], "YES", "NO",
			YES, NO, INTERCEPT_FOPEN);

	SET_INTEGER_FREE_MEMORY(cfg->intercept_freopen, config_values[15], "YES", "NO",
			YES, NO, INTERCEPT_FREOPEN);

	SET_INTEGER_FREE_MEMORY(cfg->intercept_open, config_values[16], "YES", "NO",
			YES, NO, INTERCEPT_OPEN);

	/* Should we allow the destruction of the user's libtrash configuration file? */

	SET_INTEGER_FREE_MEMORY(cfg->libtrash_config_file_unremovable, config_values[17], "YES", "NO",
			YES, NO, LIBTRASH_CONFIG_FILE_UNREMOVABLE);

	/* Are there any directories under which files should be really destroyed because they are used as mount-points
	 * for removable media? (equivalent to temporary_dirs) */

	if (config_values[18])
		cfg->removable_media_mount_points = config_values[18];

	SET_INTEGER_FREE_MEMORY(cfg->ignore_editor_temporary, config_values[19], "YES", "NO",
			YES, NO, IGNORE_EDITOR_TEMPORARY);

	/* Are there any files which would typically be covered by libtrash which the user decided to list
	 * as "exceptions"? */

	if (config_values[20])
		cfg->exceptions = config_values[20];

	if (config_values[21])
		cfg->ignore_re = config_values[21];

	/* check if PRESERVE_FILES_LARGER_THAN is specified and convert to unsigned long long */

	cfg->preserve_files_larger_than_limit = 0; // unless we can successfully read and convert a different value (below), this will default to 0 (which means no max file size)

	if (config_values[22])
	{
		unsigned long long preserve_files_larger_than_limit = 0;

		int len_of_string = strlen(config_values[22]);

		if (len_of_string >= 1 && config_values[22][0] != '-') // make sure user didn't screw up and entered a negative number (plus ensure len_of_string >= 1 makes it safe to access [len_of_string-1] below)
		{
			char m_or_g = config_values[22][len_of_string-1]; // remember the megabyte or gigabyte suffix

			if ( m_or_g == 'M' || m_or_g == 'm' ||  m_or_g == 'G' || m_or_g == 'g') // has a valid suffix
			{
				config_values[22][len_of_string-1] = '\0'; // chop off the suffix
				char* end; /* used for strtoll to signal whether the whole string was converted */

				errno = 0; // required for us to be able to properly check whether strtoll succeeded

				preserve_files_larger_than_limit = strtoll(config_values[22], &end, 10);

				if (errno == 0 && *end == '\0') // strtoll() is happy with the string we passed it (meaning: it was a valid long long)
				{
					if ( m_or_g == 'M' || m_or_g == 'm' )
					{
						preserve_files_larger_than_limit *= 1048576L;
					}
					else // means that ( m_or_g == 'G' || m_or_g == 'g' ), since we already performed test that it is one of (M,m,G,g) above
					{
						preserve_files_larger_than_limit *= 1073741824L;
					}

					// success!
					cfg->preserve_files_larger_than_limit = (unsigned long long) preserve_files_larger_than_limit; /* set max file size */
				}
				else // something went wrong
				{
#ifdef DEBUG
					fprintf(stderr,"libtrash warning: Invalid PRESERVE_FILES_LARGER_THAN setting in libtrash.conf: %s%c. Ignored.\n",
							config_values[22],m_or_g);
#endif
					;
				}

			}
			else // invalid suffix (something other than (M,m,G,g) )
			{
#ifdef DEBUG
				fprintf(stderr,"libtrash warning: Invalid suffix used for PRESERVE_FILES_LARGER_THAN setting in libtrash.conf. Ignored.\n");
#endif
				;
			}
		}
		else // negative value provided by the user or an empty string (latter shouldn't happen given how read_config_from_file works, but...!)
		{
#ifdef DEBUG
			fprintf(stderr,"libtrash warning: Negative or empty value for PRESERVE_FILES_LARGER_THAN setting in libtrash.conf. Ignored.\n");
#endif
			;
		}
	}

	/* Done. */

	/* We no longer need the config_values array, since we already copied/used
	   the pointers it held: */

	free(config_values);

	/* Done: */

	return;
}

/* ----------------------------------------------------------------------------------- */

/* decide_action() takes an absolute canonical path and the current config settings as its arguments and indicates
   to the caller how this file should be handled. It returns one of three possible values:
 *
 - BE_REMOVED: according to the user's preferences, this file can be removed without having
 a backup copy stored in her trash can first;
 *
 - BE_SAVED: according to the user's preferences, this file should be saved in the trash
 can before any further action;
 *
 - BE_LEFT_UNTOUCHED: according to the user's preferences, this file shouldn't be changed
 at all and the caller should return an error code, refusing to proceed. */

int decide_action(const char *absolute_path, config *cfg)
{
	const char *tmp = NULL;

	/* Tell the caller to handle the files already under the user's trash can according to the
	   value of cfg->protect_trash, also taking into consideration whether (or not) the trash can
	   is currently listed in UNCOVER_DIRS: */

	if (found_under_dir(absolute_path, cfg->absolute_trash_can))
	{
		if (cfg->protect_trash == NO ||
				found_under_dir(absolute_path, cfg->uncovered_dirs)) /* user temporarily disabled PROTECT_TRASH via UNCOVER_DIRS */
			return BE_REMOVED;
		else /* if cfg->protect_trash == YES && !found_under_dir(absolute_path, cfg->uncover_dirs) */
			return BE_LEFT_UNTOUCHED;
	}

	/* Tell the caller to return an error code and don't even touch these files: */

	if ( (found_under_dir(absolute_path, cfg->unremovable_dirs) &&
				!found_under_dir(absolute_path, cfg->uncovered_dirs)  &&
				!is_an_exception(absolute_path, cfg->exceptions) ) || /* (a) */
			(cfg->libtrash_config_file_unremovable                && /* (b) */
			 (found_under_dir(absolute_path, cfg->home) && !strcmp(absolute_path + strlen(cfg->home) + 1, PERSONAL_CONF_FILE)) ) ) /* (c) */
		return BE_LEFT_UNTOUCHED;

	/* Notes:
	 * (a) - file lies in one of the cfg->unremovable_dirs, that dir hasn't been "uncovered" and this file isn't an "exception"; or
	 * (b) - we have instructions to protect the user's libtrash configuration file and
	 * (c) - we make sure that this file is in the user's home directory and then compare PERSONAL_CONF_FILE with the portion
	 * of absolute_path which _follows_ the name of the home directory and the slash which separates it from the file name.
	 */

	/* Tell the caller to remove (without saving) the following kinds of files: */

	if ( (cfg->ignore_hidden && hidden_file(absolute_path)) ||                           /* is a hidden file and we were told
											      * to ignore these; */
			(cfg->ignore_editor_backup && absolute_path[strlen(absolute_path) - 1] == '~') || /* is an editor backup file and
													   * we were told to ignore these; */

			(cfg->ignore_editor_temporary &&                                             /* is a temporary file used by a text */
			 ((tmp = strrchr(absolute_path, '/')) ? *(tmp + 1) == '#' :                  /* editor */
			  absolute_path[0] == '#') )                                            ||

			found_under_dir(absolute_path, cfg->temporary_dirs)                     ||      /* is a (normal) temporary file; */

			found_under_dir(absolute_path, cfg->user_temporary_dirs)                ||      /* is a temporary file in a dir under $HOME; */

			(!found_under_dir(absolute_path, cfg->home) && !cfg->global_protection) ||      /* is outside of the user's dir and
													   the user doesn't want us to protect
													   these files. */

			ends_in_ignored_extension(absolute_path, cfg)                           ||      /* filename ends in an extension
													 * we were told to ignore */

			(strlen(cfg->ignore_re) > 0 && matches_re(absolute_path, cfg->ignore_re)) ||   /* file name matches the IGNORE_RE */

			found_under_dir(absolute_path, cfg->removable_media_mount_points)       ||      /* file is on a removable medium */

			is_empty_file(absolute_path))                                            	/* zero byte-count in regular file */


			return BE_REMOVED;

	/* Tell the caller not to remove large files. Use TRASH_OFF=YES to override */

	if (file_is_too_large(absolute_path, cfg->preserve_files_larger_than_limit))       	/* file is bigger than the max file size limit and user */
		return BE_LEFT_UNTOUCHED;							/* wants us to refuse to move to it to the trash and return an error    */

	/* If the file doesn't fall into any of these categories, it means that it is a file which the user wants to
	   save a copy of rather than permanently destroying it; it is up to the caller to determine whether this file
	   resides in or outside of the user's home directory and act accordingly: */

	return BE_SAVED;
}

/* --------------------------------------------------------------------------- */

/* What this function does: it checks whether the user running the program has write-access
 * to the directory which holds the file named filepath and whether that
 * file doesn't reside on a read-only filesystem. It returns either 1 (if
 * write-access is allowed) or 0 (otherwise). In case of a memory
 * allocation failure, we return 2, so that if the caller just tests our
 * return value for a non-null value it matches and the caller proceeds
 * with its tasks without aborting (full explanation in rename.c): */

int can_write_to_dir(const char *filepath)
{
	const char *slash = NULL;

	uid_t real_uid, effective_uid;

	int error = 0;

	int retval = 0;

	int previous_errno = 0;

	/* First of all: so that SETUID programs can run without any problems, we must perform this
	 * check with the _effective_ uid rather than the real uid. To do so we must temporarily set
	 * the real uid to the value of the effective uid, call access() (it uses the real uid for
	 * performing permissions checks) and then restore the original value of the real uid. */

	/* Determine real and effective uids: */

	real_uid      = getuid();

	effective_uid = geteuid();

	/* Set real uid to the value of effective_uid: */

	error = setreuid(effective_uid, -1);

	/* In case of failure, we return 2 right away: running access() with the real uid unchanged might
	 * hinder the correct functioning of SETUID programs, and without running access() we can't determine
	 * if the user has write-access to this dir: */

	if (error)
		return 2;

	/* Now we perform the access() checks, not forgetting to restore the original real_uid before returning: */

	slash = strrchr(filepath, '/');

	/* If no slash was found, this file is in the cwd and that's the directory we should focus on: */

	if (slash == NULL)
	{
		if (access(".", W_OK))
			retval = 0;
		else /* if (!access(".", W_OK)) - we have write-permission: */
			retval = 1;
	}
	else /* if (slash != NULL) */
	{

		/* Create temporary string which holds the name of the directory: */

		char *dir_name = alloca(slash - filepath +
				(slash == filepath ? 1 : 0) + /* if filepath has the form "/filename", we need one byte for */
				1);                           /* the slash */

		if (!dir_name) /* alloca() failed, just set retval to 2: */
		{
#ifdef DEBUG
			fprintf(stderr, "can_write_to_dir(): allocation failure.\n");
#endif
			retval = 2;
		}
		else /* alloca() succeeded: */
		{
			/* Copy the name of the directory into dir_name and null-terminate it: */

			if (slash != filepath)
			{
				strncpy(dir_name, filepath, slash - filepath);
				dir_name[slash - filepath] = '\0';
			}
			else /* special case in which slash points to the file system's root (slash == filepath): */
			{
				dir_name[0] = '/';
				dir_name[1] = '\0';
			}

			if (access(dir_name, W_OK))
				retval = 0;
			else /* if (!access(dir_name, W_OK)) - we have write-permission: */
				retval = 1;
		}
	}

	/* Now that retval is set, we only need to restore the original
	 * value of the real uid. However, before doing so, and only if retval
	 * holds 0 (i.e., if write-access is impossible), we store the previous
	 * errno so that we can later restore it, because, if setreuid() fails, we
	 * still wish that, upon this our return, errno defines the reason why
	 * write access to this file's directory isn't allowed (instead of
	 * defining the reason why setreuid() failed): */

	if (retval == 0) /* no write-access */
		previous_errno = errno;

	error = setreuid(real_uid, -1);

	if (retval == 0)
		errno = previous_errno; /* restore the value which "explains" why we don't have write-access to this file's directory */

#ifdef DEBUG
	if (error) /* we must leave the process running with the wrong real uid - or is there something else we can do? */
		fprintf(stderr, "can_write_to_dir(): unable to restore original uid.\n");
#endif

	/* Done:  */

	return retval; /* independently of the successfulness of the call to setreuid(), if write-access is impossbible errno's
			* value holds the "explanation". */
}

/* -------------------------------------------------------------------- */

/* This function is used by graft_file() if the real rename() fails to "move" a file to the trash can
 * because that file resides on a filesystem different from the one which holds the user's trash can.
 * If performs the following operations:
 *
 * (z) - it checks whether the user can write to the directory
 * which holds old_path, and returns -2 if she can't; *)
 *
 * (a) - it creates/opens in write-mode a file called new_path and opens
 * the file old_path in read-mode (returns -2 if the latter fails due to
 * insufficient permissions);
 *
 * (b) - it copies each byte in file old_path to file new_path;
 *
 * (c) - it closes both files;
 *
 * (d) - it ("really") unlink()s the file at old_path.
 *
 * It returns 0 if all these operations (a-d) succeed, -1 otherwise.
 *
 * *) Why do we do that? Because, if the user lacks is unable to write to
 * the directory under which the file called old_path resides, we will make
 * an unnecessary copy of that file: we will first create the duplicate
 * file and then notice that we don't have sufficient permissions to be
 * able to unlink() the file at its original location (or that we are on a
 * RO file-system) . For that reason, we return an error code right away if
 * we determine that unlink()ing the original file will prove itself
 * impossible. Why -2 rather than -1, like we do in all other error
 * situations? Because graft_file() (our caller) needs to be able to tell
 * if we failed due to this specific problem. Why does graft_file() need to
 * know this? Because unlink(), when calling graft_file(), needs to know
 * this. Why? Because it must set errno either to 0 or to
 * EACCES/EPERM/EROFS, depending on the cause of graft_file()'s failure. */

static int move(const char *old_path, const char *new_path, config *cfg)
{
	FILE *old_file = NULL, *new_file = NULL;

	int byte_written = 0, byte_read = 0;

	int error1 = 0, error2 = 0;

	/* First of all: check if we can write to the directory which holds old_path, return a different
	   error code if we can't (see explanation above): */

	if (!can_write_to_dir(old_path))
	{
#ifdef DEBUG
		fprintf(stderr, "move() returning error code because we can't write to the directory under which %s"
				" resides.\n", old_path);
#endif

		return -2; /* can_write_to_dir() has set errno to a relevant value */
	}

	/* Make sure that we are able to use the real fopen(): */

	if (!cfg->real_fopen)
	{

#ifdef DEBUG
		fprintf(stderr, "real_fopen is not available, bailing out! (move()).\n");
#endif

		return -1;
	}

	/* Open both files, exit upon error: */

	old_file = (cfg->real_fopen) (old_path, "rb");

	if (!old_file)
	{
#ifdef DEBUG
		fprintf(stderr, "move() unable to open file at old_path for reading.\n");
#endif
		if (errno == EACCES)
			return -2; /* special error code if we failed due to insufficient permissions */
		else
			return -1;
	}

	new_file = (cfg->real_fopen) (new_path, "wb");

	if (!new_file)
	{
#ifdef DEBUG
		fprintf(stderr, "move() unable to open file at new_path for writing.\n");
#endif
		fclose(old_file); /* try to close successfully opened file */

		return -1;
	}

	/* Copy byte-after-byte until either no more characters are available for reading (due to EOF or
	 * IO error) or a write-error occurs: */

	while ( (byte_read = fgetc(old_file)) != EOF &&
			(byte_written = fputc(byte_read, new_file)) != EOF )
		;

	/* We exited the loop; but why? */

	if (byte_read == EOF) /* we exited the loop because no character could be read ... */
	{
		if (feof(old_file) && !ferror(old_file)) /* due to a "real" EOF; everything's OK */
			error1 = 0;
		else                                     /* due to an error while reading; signal failure */
			error1 = 1;
	}
	else /* we exited the loop due to a write-error (byte_written == EOF) */
		error1 = 1;

	/* Anyway, try to close both files: */

	fclose(old_file); /* A failure to close this file should result neither in data loss (since the file wasn't open for
			   * writing) nor in undefined behaviour at the time of the call to unlink(), since the man pages states
			   * that "if the name was the last link to a file but any processes still have the file open the file will
			   * remain in existence until the last file descriptor referring to it is closed." For that reason, it is
			   * ignored. Should we handle this in a different way? */

	error2 = fclose(new_file); /* On the other hand, failing to close the file we had opened in write-mode does lead us to
				    * return an error code. */

	/* If either (a) a IO error occurred while copying or (b) we couldn't close the new_file, we return -1 right away: */

	if (error1 || error2)
		return -1;

	/* Else (i.e., if we are sure that a copy of old_file is currently available at new_path),
	 * try to ("really") unlink() the old file and return 0 if the real unlink() succeeds, -2 if it fails due to EACCES,
	 * EPERM or EROFS, and -1 if it fails for any other reason: */

	error1 = (cfg->real_unlink) (old_path); /* once again, should we worry about the possibility of unlink()ing a currently open
						 * file? (possible since we don't abort if the first call to fclose() fails) */

	if (error1 &&
			(errno == EACCES || /* We have already done this tests above, trying to prevent a unnecessary copy from being made. */
			 errno == EPERM  || /* But if something changed since then, must return -2 nonetheless to graft_file(). */
			 errno == EROFS) )
		return -2;
	else /* either unlink() succeeded or it failed for some other reason: */
		return error1;
}

char * convert_relative_into_absolute_paths(const char *relative_paths)
{
	struct passwd *userinfo = NULL;

	char *new_list = NULL;

	const char *orig_ptr = NULL;
	char *new_ptr = NULL;

	unsigned int semicolon_count = 0;

	userinfo = getpwuid(geteuid());

	/* If it failed, we just return  NULL: */

	if (!userinfo)
	{
#ifdef DEBUG
		fprintf(stderr, "Unable to determine information about the user.\n");
#endif
		return NULL;
	}

	/* To determine the size of the buffer we must allocate, we must know how many semicolons there are in our argument: */

	orig_ptr = relative_paths;

	while (*orig_ptr != '\0')
	{
		if (*orig_ptr == ';')
			semicolon_count++;
		orig_ptr++;
	}

	/* We allocate space for (i) the entire string we were passed, (ii) a trailing null char and (iii) a string holding
	 * the user's home dir and a slash for each relative path in our argument (the number of paths is given by semicolon_count+1): */

	new_list = malloc(strlen(relative_paths) + 1 + (semicolon_count + 1) * (strlen(userinfo->pw_dir) + 1));

	if (!new_list)
	{
#ifdef DEBUG
		fprintf(stderr, "Memory allocation error inside convert_relative_into_absolute_paths().\n");
#endif
		return NULL;
	}

	/* Now we iterate along our argument, and copy its contents into the buffer new_list, always prefixing each dir name with the user's
	 * home directory: */

	orig_ptr = relative_paths;
	new_ptr = new_list;

	/* First dir name also gets a prefix: */

	strcpy(new_ptr, userinfo->pw_dir);
	new_ptr += strlen(userinfo->pw_dir);
	*new_ptr++ = '/';

	while (*orig_ptr != '\0')
	{
		if (*orig_ptr != ';')
			*new_ptr = *orig_ptr;
		else //if (*orig_ptr == ';')
		{
			*new_ptr++ = ';';
			strcpy(new_ptr, userinfo->pw_dir);
			new_ptr += strlen(userinfo->pw_dir);
			*new_ptr = '/';
		}

		orig_ptr++;
		new_ptr++;
	}

	*new_ptr = '\0';

	return new_list;
}

/* This function is used by decide_action to determine whether a
 * file is empty or not. It returns 1 if the file at path (i) exists, (ii) is a regular file
 * (or a symlink to one) and (iii) is empty, 0 otherwise. */

static int is_empty_file(const char *path)
{
	struct stat file_stat;
	int retval;

	retval = stat(path, &file_stat);

	if (retval == -1 || /* stat() failed (so err on the side of caution) */
			(!S_ISREG(file_stat.st_mode) && !S_ISLNK(file_stat.st_mode)) || /* path is neither a regular file nor a symlink */
			file_stat.st_size > 0) /* is not empty (0 bytes) */
		return 0;
	else
		return 1;

}

/* This function will test the file size against the PRESERVE_FILES_LARGER_THAN
 * configuration setting. Will return 1 if PRESERVE_FILES_LARGER_THAN was defined
 * by the user and (either the file exceeds that limit or an error occurs).
 * Returns 0 otherwise (i.e.: either PRESERVE_FILES_LARGER_THAN was not defined or
 * we succeeded in determining that the file is smaller than that limit).
 */

static int file_is_too_large(const char *path, unsigned long long preserve_files_larger_than_limit)
{
	struct stat file_stat;
	int retval;

	if (preserve_files_larger_than_limit == 0) // PRESERVE_FILES_LARGER_THAN feature is not in use, leave immediately (no need to do lstat() call)
		return 0;

	/* Let us get the size of this file and see if it exceeds the limit defined by the user: */

	retval = lstat(path, &file_stat);

#ifdef DEBUG
	fprintf(stderr, "Return value: %d, errno: %d, File Stat Size: %llu, Max File Size: %llu\n",
			retval, errno, file_stat.st_size, preserve_files_larger_than_limit);
#endif

	if (retval == -1 || file_stat.st_size >= preserve_files_larger_than_limit) /* lstat() call failed OR file is too large => we won't remove this file */
		return 1;
	else /* lstat() call succeeded and we have established this file is below the PRESERVE_FILES_LARGER_THAN defined by the user */
		return 0;
}

/* The following two user-contributed functions implement support for the IGNORE_RE
   feature. */

#ifdef DEBUG
static void regex_report_error (int errcode, regex_t *compiled)
{
	size_t  len    = regerror (errcode, compiled, NULL, 0);
	char   *buffer = malloc (len + 1);

	regerror (errcode, compiled, buffer, len);
	fprintf (stderr, "%s\n", buffer);
	free (buffer);
}
#endif

static int matches_re (const char *absolute_path, const char *regexp)
{
	regmatch_t matches[1];
	regex_t    compiled;
	int        ret;

	ret = regcomp (&compiled, regexp, REG_EXTENDED);

	if (ret)
	{
#ifdef DEBUG
		regex_report_error (ret, &compiled);
#endif
		regfree (&compiled);
		return 0;
	}

	ret = regexec (&compiled, absolute_path, 1, matches, 0);

#ifdef DEBUG
	if (ret && ret != REG_NOMATCH)
	{
		regex_report_error (ret, &compiled);
	}
	if (ret == 0)
	{
		fprintf (stderr, "file %s matches %s\n", absolute_path, regexp);
	}
#endif

	regfree (&compiled);

	return (ret == 0);
}


/* This function is used by the wrappers of the *at() functions. It "resolves"
 * the dirfd+path pair which it is passed according to the rules used by those
 * functions do. More specifically:
 *
 * - passed a file descriptor to a dir and a *relative* path, it returns the
 *   absolute path to the file called arg_pathname located under the dir referred
 *   to by dirfd.
 * - passed an *absolute* arg_pathname, then it returns a pointer to its own
 *   argument.
 * - passed dirfd==AT_FDCWD, then it returns a pointer to its own argument.
 *
 * (This is how the *at() functions handle their path+dirfd arguments: dirfd
 * is only taken into account if (i) it is != AT_FDCWD and (ii) the arg_pathname
 * is relative.) Since we don't always return a malloc()ed buffer (might return
 * a pointer to the arg_pathname arg!), calling code must check if
 * retval != arg_pathname before free()ing that memory.
 *
 * It returns a pointer to (i) a malloc()ed buffer containing the desired path,
 * (ii) a pointer to its own arg or (iii) NULL in case of error (in which case
 * errno might also have been set to something meaningful). In case (i), the
 * caller must call free() on the returned pointer.
 *
 * It relies on the /proc filesystem to figure out what the dirfd arg refers to.
 *
 */

#ifdef AT_FUNCTIONS

char* make_absolute_path_from_dirfd_relpath(int dirfd, const char *arg_pathname)
{
	char *abs_path = NULL;

	if (arg_pathname == NULL)
	{
		return NULL;
	}
	else if (arg_pathname[0] == '/' || dirfd == AT_FDCWD)
	{
		return arg_pathname;
	}
	else if (dirfd < 0)
	{
		errno = EBADF;
		return NULL;
	}

	/* need to resolve path "relative" to whatever dirfd points to */

	char first_part_of_proc_path[] = "/proc/self/fd/";

	int pathlen = strlen(first_part_of_proc_path) + (ilog10(dirfd) + 1) + 1;

	char path_to_proc_fd[pathlen];

	int chars_written = snprintf(path_to_proc_fd, pathlen, "%s%d", first_part_of_proc_path, dirfd);

	if (chars_written != pathlen -1)
	{
		fprintf(stderr, "[libtrash] BUG!! apparently calculated wrong string len when accessing /proc/self/fd/... path\n");
		errno = 0;
		return NULL;
	}

	char *first_part_of_abs_path = canonicalize_file_name(path_to_proc_fd); /* this mem must be FREE()d before we return */

	if (first_part_of_abs_path == NULL) /* we assume this failure is due to being passed a bad dirfd */
	{
		errno = EBADF;
		return NULL;
	}

	/* IMPORTANT note: from this point onwards, we must free(first_part_of_abs_path) before returning */

	/* test it is a dir */

	int error = 0;

	struct stat st;

	error = stat(first_part_of_abs_path, &st);

	if (error)
	{
		free(first_part_of_abs_path); /* IMPORTANT! */
		errno = EBADF;
		return NULL;
	}

	if (!S_ISDIR(st.st_mode))
	{
		free(first_part_of_abs_path); /* IMPORTANT! */
		errno = ENOTDIR;
		return NULL;
	}

	/* remove trailing slash, in case there is one */

	if (strlen(first_part_of_abs_path) > 0 &&
			first_part_of_abs_path[strlen(first_part_of_abs_path)-1] == '/')
	{
		first_part_of_abs_path[strlen(first_part_of_abs_path)-1] = '\0';
	}

	/* now simply put together abs_path by concatenating first_part_of_abs_path and our arg_pathname */

	abs_path = malloc(strlen(first_part_of_abs_path) + 1 /* for the slash */ + strlen(arg_pathname) + 1);

	if (abs_path == NULL)
	{
		free(first_part_of_abs_path); /* IMPORTANT! */
		errno = ENOMEM;
		return NULL;
	}

	strcpy(abs_path, first_part_of_abs_path);
	strcat(abs_path, "/");
	strcat(abs_path, arg_pathname);
	free(first_part_of_abs_path); /* IMPORTANT! */
	return abs_path;
}

#endif


/* This function, get_real_function, takes as its only argument  one of the
 * function-name macros defined in trash.h  and returns a pointer to that function.
 * We do NOT use dlsym() but instead a fancier more cumbersome system relying on
 * (i) dlvsym() and (ii) knowledge of the version of the corresponding version to
 * which the run-time linker would actually link. This avoids the problem inherent
 * in the way dlsym() (no 'v') behaves: when asked for 'fopen', it will give us a
 * pointer to the older version of that function present in GNU libc, while run-time
 * linking of a program invokign fopen() would get the most recent version of fopen()
 * found in the system's GNU libc to be executed.
 * Returns NULL in case of error/being unable to get a pointer to the specified
 * function. */

void* get_real_function(int function_name)
{
	dlerror();

	void *p = NULL;

	switch (function_name)
	{
		case UNLINK: p = dlvsym(RTLD_NEXT, "unlink", UNLINK_VERSION);
			     break;

		case RENAME: p = dlvsym(RTLD_NEXT, "rename", RENAME_VERSION);
			     break;

		case FOPEN: p = dlvsym(RTLD_NEXT, "fopen", FOPEN_VERSION);
			    break;

		case FOPEN64: p = dlvsym(RTLD_NEXT, "fopen64", FOPEN64_VERSION);
			      break;

		case FREOPEN: p = dlvsym(RTLD_NEXT, "freopen", FREOPEN_VERSION);
			      break;

		case FREOPEN64: p = dlvsym(RTLD_NEXT, "freopen64", FREOPEN64_VERSION);
				break;

		case OPEN: p = dlvsym(RTLD_NEXT, "open", OPEN_VERSION);
			   break;

		case OPEN64: p = dlvsym(RTLD_NEXT, "open64", OPEN64_VERSION);
			     break;
	}

	if (dlerror())
		p = NULL;

	return p;
}
