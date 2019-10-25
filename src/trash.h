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

/* Must be put here so that the pointer to fopen can be declared in this file: */

#include <stdio.h>

/* Various macros which are supposed to make the code more readable: */

#define ALLOW_DESTRUCTION  1
#define PROTECT            0

#define BE_REMOVED         1
#define BE_SAVED           2
#define BE_LEFT_UNTOUCHED  3

#define YES                1
#define NO                 0

#define NOMEM              1
#define FERROR             2
#define REALLOC_FACTOR     2  /* defines by how much we multitply the size of a buffer when it needs to be reallocated */


#define UNLINK       1
#define RENAME       2
#define FOPEN        3
#define FOPEN64      4
#define FREOPEN      5
#define FREOPEN64    6
#define OPEN         7
#define OPEN64       8
#define OPENAT       9
#define OPENAT64    10

/* You probably don't want to change this value, unless you spend _lots_ of time deleting
 * files with the same name in the same dir, and  you have that dir covered by libtrash.
 * (Even then, changing this value is _NOT_ necessary.)
 */

#define DEF_DIGITS 3

/* -------------------------------------------------------------- */

/* Define a structure which holds all configuration settings: */

typedef struct
{
   /* Configuration variables: */
   
   int in_case_of_failure;
   int global_protection;
   int should_warn;
   int ignore_hidden;
   int ignore_editor_backup;
   int ignore_editor_temporary;
   int protect_trash;
   int libtrash_config_file_unremovable;
   
   int libtrash_off;
   int general_failure;
   
   int intercept_unlink;
   int intercept_rename;
   int intercept_fopen;
   int intercept_freopen;
   int intercept_open;

   /* we store pointers for these three (but not the rest) because the code in
    libtrash actually needs these three functions. */
   
   int (*real_unlink) (const char*);
   int (*real_rename) (const char*, const char*);
   FILE* (*real_fopen) (const char*, const char*);
   
   char *ignore_extensions;
   char *relative_trash_can;
   char *relative_trash_system_root;
   char *unremovable_dirs;
   char *uncovered_dirs;
   char *temporary_dirs;
   char *user_temporary_dirs;
   char *removable_media_mount_points;
   char *exceptions;
   char *ignore_re;
   char *absolute_trash_can;
   char *absolute_trash_system_root;
   char *home;
   /* check for max file size. 0 means don't check */
   unsigned long max_file_size;
}
config;

/* Initialization/exit routines: */

void libtrash_init(config * cfg);

void libtrash_fini(config * cfg);

/* Helper functions (defined in helpers.c):  */

char * convert_relative_into_absolute_paths(const char *relative_paths);

int found_under_dir(const char *absolute_path, const char *dir_list);

int dir_ok(const char *pathname, int *name_collision);

int graft_file(const char *new_top_dir, const char *old_path, const char *what_to_cut, config *cfg);

int hidden_file(const char *absolute_path);

int ends_in_ignored_extension(const char *pathname, config *cfg);

char* build_absolute_path(const char *path, int should_follow_final_symlink);

int decide_action(const char *absolute_path, config *cfg);

int can_write_to_dir(const char *filepath);

void get_config_from_file(config *cfg);

char* make_absolute_path_from_dirfd_relpath(int dirfd, const char *arg_pathname);

void* get_real_function(int function_name);

/* -------------------------------------------------------------------------------------------- */

