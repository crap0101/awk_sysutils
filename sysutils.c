/*
Copyright (C) 2023,  Marco Chieppa | crap0101

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.
*/

/*
 * Description: system utilities for gawk.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gawkapi.h"

// define these before include awk_extensions.h
#define _DEBUGLEVEL 0
#define __module__ "sysutils"
#define __namespace__ "sys"
static const gawk_api_t *api;
static awk_ext_id_t ext_id;
static const char *ext_version = "0.1";

// ... and now include other own utilities
#include "awk_extensions.h"
// https://github.com/crap0101/laundry_basket/blob/master/awk_extensions.h


static awk_value_t * do_check_path(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_getcwd(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_get_pathsep(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_mktemp(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_rm(int nargs, awk_value_t *result, struct awk_ext_func *finfo);

/* ----- boilerplate code ----- */
int plugin_is_GPL_compatible;

static awk_ext_func_t func_table[] = {
  { "check_path", do_check_path, 2, 1, awk_false, NULL },
  { "getcwd", do_getcwd, 0, 0, awk_false, NULL },
  { "get_pathsep", do_get_pathsep, 0, 0, awk_false, NULL },
  { "mktemp", do_mktemp, 1, 0, awk_false, NULL },
  { "rm", do_rm, 1, 1, awk_false, NULL },
};

__attribute__((unused)) static awk_bool_t (*init_func)(void) = NULL;


int dl_load(const gawk_api_t *api_p, void *id) {
  api = api_p;
  ext_id = (awk_ext_id_t) &id;
  int errors = 0;
  long unsigned int i;
  
  if (api->major_version < 3) { //!= GAWK_API_MAJOR_VERSION
      //    || api->minor_version < GAWK_API_MINOR_VERSION) {
    eprint("incompatible api version:  %d.%d != %d.%d (extension/gawk version)\n",
	   GAWK_API_MAJOR_VERSION, GAWK_API_MINOR_VERSION, api->major_version, api->minor_version);
    exit(1);
  }
  
  for (i=0; i < sizeof(func_table) / sizeof(awk_ext_func_t); i++) {
    if (! add_ext_func(__namespace__, & func_table[i])) {
      eprint("can't add extension function <%s>\n", func_table[0].name);
      errors++;
    }
  }
  if (ext_version != NULL) {
    register_ext_version(ext_version);
  }
  return (errors == 0);
}

/* ----- end of boilerplate code ----------------------- */


/*********************/
/* UTILITY FUNCTIONS */
/*********************/

String
path_join(String first, String last)
{
  /*
   * Joins the two path components $first and $last in a
   * newly allocated String, which is returned if the process
   * succedes. If fails, returns NULL.
   */
  String joined = NULL;
  if (NULL == (joined = alloc_string(joined, 1 + strlen(_PATHSEP) + strlen(first) + strlen(last)))) {
    eprint("%s", strerror(errno));
    return NULL;
  }
  joined = strncpy(joined, first, strlen(first));
  joined[strlen(first)] = '\0';
  if (strncmp(& first[strlen(first)-1], _PATHSEP, 1)) {
    /* NOTE_W: 
     * wtf???
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83404
     * tempted to use strcat to silence the senseless
     * `specified bound 1 equals source length [-Wstringop-overflow=` warning
     * since the allocated space is surely enought... but, change
     * the defines in awk_extension.h is enough, and maybe even clearer.
     */
    joined = strncat(joined, _PATHSEP, strlen(_PATHSEP));
  }
  joined = strncat(joined, last, strlen(last));
  return joined;
}


int
check_path(String path, int mask)
{
  /*
   * Loosly check if a file/dir exists and is readable and writable.
   * see <man 2 access> for details.
   * $mask can be R_OK, W_OK, X_OK or a bitwise of them.
   */
  return access(path, mask);
}


String
get_current_dir(String dest, size_t size)
{
  /*
   * Writes on $dest the path of the current working directory,
   * or NULL if fails.
   * $size is the initial size to allocate for $dest.
   * Returns $dest.
   */
  while (1) {
    if (NULL == (dest = getcwd(dest, size))) {
      if (errno == ERANGE) {
	size *= 2;
	if (NULL == (dest = alloc_string(dest, size))) {
	  eprint("%s", strerror(errno));
	  return NULL;
	}
      } else {
	eprint("%s", strerror(errno));
	return NULL;
      }
    } else {
      break;
    }
  }
  return dest;
}
		       
/***********************/
/* EXTENSION FUNCTIONS */
/***********************/

static awk_value_t*
do_check_path(int nargs,
	      awk_value_t *result,
	      __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
   * Loosely checks if a file/dir exists and is readable *and* writable.
   * Return true if success, 0 otherwise (non-existent, no permissions, etc.).
   * see <man 2 access> for details.
   * Accetps one or two arguments. The 1st must the path to check,
   * the (optional) 2nd arg must a string representing the file mode
   * (readable, writable, ...), written as a combination of "r" "w" "x".
   * The latter default to "r".
   */
  awk_value_t path;
  awk_value_t mask_s;
  size_t i, mask = 0;
  assert(result != NULL);
  make_number(0, result);

  if (nargs > 2)
    fatal(ext_id, "too many arguments\n");

  if (! get_argument(0, AWK_STRING, & path))
    fatal(ext_id, "can't retrieve path\n");

  if (nargs == 2) {
      if (! get_argument(1, AWK_STRING, & mask_s))
	fatal(ext_id, "can't retrieve mask\n");

      for (i=0; i<mask_s.str_value.len; i++) {
	switch (mask_s.str_value.str[i]) {
	case 'r': mask |= R_OK; break;
	case 'w': mask |= W_OK; break;
	case 'x': mask |= X_OK; break;
	default:
	  fatal(ext_id, "Unknown mask value <%c>\n", mask_s.str_value.str[i]);
	}
      }
  } else {
    mask = R_OK;
  }

  if (-1 == check_path(path.str_value.str, mask)) {
    eprint("<%s> %s\n", path.str_value.str, strerror(errno));
    goto out;
  }

  make_number(1, result);
 out:
  return result;
}

static awk_value_t*
do_getcwd(int nargs,
	  awk_value_t *result,
	  __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
   * Returns the path of the current working directory,
   * or an empty string if fails.
   */
  String pathname;
  String current_dir = NULL;
  size_t dir_size = 100;

  assert(result != NULL);
  emalloc(pathname, String, sizeof(""), __func__);
  strcpy(pathname, "");
  make_malloced_string(pathname, strlen(pathname), result);

  if (nargs > 0)
    fatal(ext_id, "%s takes no arguments\n", __func__);

  if (NULL == (current_dir = get_current_dir(current_dir, dir_size))) {
    eprint("can't retrieve current dir\n");
    goto out;
  }
  
  erealloc(pathname, String, strlen(current_dir)+1, __func__);
  strcpy(pathname, current_dir);
  make_malloced_string(pathname, strlen(pathname), result);

 out:
  free(current_dir);
  return result;
}


static awk_value_t*
do_get_pathsep(int nargs,
	  awk_value_t *result,
	  __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
   * Returns the system's path separator.
   */
  String ps = NULL;

  assert(result != NULL);
  if (nargs > 0)
    fatal(ext_id, "%s takes no arguments\n", __func__);
  
  erealloc(ps, String, strlen(_PATHSEP)+1, __func__);
  strcpy(ps, _PATHSEP);
  make_malloced_string(ps, strlen(ps), result);
  return result;
}


static awk_value_t*
do_mktemp(int nargs,
	  awk_value_t *result,
	  __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
  * Returns the path of a just created temporary file,
  * or the empty string if fails.
  * By default the temporary file is created in the current working directory
  * but, if $nargs[0] is provided, this's must be a path to another directory
  * in which the temporary file will be placed.
  * See <man 3 mkstemp> for details.
  */
  String pathname = NULL;
  String currdir = NULL;
  String fullpath = NULL;
  String cwd = NULL;
  size_t currdirsize = 100;
  awk_value_t tmp_dir;
  int fd;
  char template[] = "tmp_XXXXXX";

  umask(S_IWGRP | S_IWOTH);

  assert(result != NULL);
  emalloc(pathname, String, sizeof(""), __func__);
  strcpy(pathname, "");
  make_malloced_string(pathname, strlen(pathname), result);
  
  if (nargs > 1) {
    fatal(ext_id, "%s: too many arguments\n", __func__);
  } else if (nargs == 1) {
    if (! get_argument(0, AWK_STRING, & tmp_dir))
      fatal(ext_id, "can't retrieve dir\n");

    cwd = tmp_dir.str_value.str;
  } else {
    if (NULL == (currdir = get_current_dir(currdir, currdirsize))) {
      eprint("can't retrieve current dir\n");
      goto out;
    }
    cwd = currdir;
  }

  if (NULL == (fullpath = path_join(cwd, template))) {
    eprint("can't build temp path\n");
    goto out;
  }
  
  if (-1 == (fd = mkstemp(fullpath))) {
    eprint("mkstemp failed: %s", strerror(errno));
    goto out;
  } else {
    if (-1 == close(fd))
      eprint("close failed: %s", strerror(errno));
  }

  dprint("create temp file <%s>\n", fullpath);
  erealloc(pathname, String, strlen(fullpath)+1, __func__);
  strcpy(pathname, fullpath);
  make_malloced_string(pathname, strlen(pathname), result);
  
 out:
  free(currdir);
  free(fullpath);
  return result;
}

static awk_value_t*
do_rm(int nargs,
      awk_value_t *result,
      __attribute__((unused)) struct awk_ext_func *finfo)
{
  /*
  * Removes the $nargs[0] file (or directory, if empty)
  * using the remove system call.
  * See <man 3 remove> for details.
  * Returns true if success, else false.
  */
  assert(result != NULL);
  make_number(0.0, result);
  awk_value_t pathname;
  if (nargs != 1)
    fatal(ext_id, "too many arguments! One expected: path_to_file_or_dir\n");

  if (! get_argument(0, AWK_STRING, & pathname)) {
    if (pathname.val_type != AWK_STRING)
      fatal(ext_id,"wrong type argument: <%s> (expected: <%s>)\n",
	    _val_types[pathname.val_type], name_to_string(AWK_STRING));
    else
      fatal(ext_id, "can't retrieve path <%s>\n", pathname.str_value.str);
  }

  if (-1 == remove(pathname.str_value.str)) {
    eprint("<%s> %s\n", pathname.str_value.str, strerror(errno));
    goto out;
  }
  
  make_number(1, result); 
 out:
  return result;
}

/* COMPILE WITH (me, not necessary you):
gcc -fPIC -shared -DHAVE_CONFIG_H -c -O -g -I/usr/include -iquote ~/local/include/awk -Wall -Wextra sysutils.c && gcc -o sysutils.so -shared sysutils.o && cp sysutils.so ~/local/lib/awk/
*/
