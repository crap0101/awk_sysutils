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

#include "awk_extensions.h"
// https://github.com/crap0101/laundry_basket/blob/master/awk_extensions.h

static awk_value_t * do_check_path(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_getcwd(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_mktemp(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_rm(int nargs, awk_value_t *result, struct awk_ext_func *finfo);

/* ----- boilerplate code ----- */
int plugin_is_GPL_compatible;
static const gawk_api_t *api;

static awk_ext_id_t ext_id;
static const char *ext_version = "0.1";

static awk_ext_func_t func_table[] = {
  { "check_path", do_check_path, 2, 1, awk_false, NULL },
  { "getcwd", do_getcwd, 0, 0, awk_false, NULL },
  { "mktemp", do_mktemp, 1, 0, awk_false, NULL },
  { "rm", do_rm, 1, 1, awk_false, NULL },
};

__attribute__((unused)) static awk_bool_t (*init_func)(void) = NULL;


int dl_load(const gawk_api_t *api_p, void *id) {
  api = api_p;
  ext_id = (awk_ext_id_t) &id;
  int errors = 0;
  long unsigned int i;
  
  if (api->major_version != GAWK_API_MAJOR_VERSION
      || api->minor_version < GAWK_API_MINOR_VERSION) {
    eprint("incompatible api version:  %d.%d != %d.%d (extension/gawk version)\n",
	   GAWK_API_MAJOR_VERSION, GAWK_API_MINOR_VERSION, api->major_version, api->minor_version);
    exit(1);
  }
  
  for (i=0; i < sizeof(func_table) / sizeof(awk_ext_func_t); i++) {
    if (! add_ext_func("sys", & func_table[i])) {
      eprint("can't add extension function <%s>\n", func_table[0].name);
      errors++;
    }
  }
  if (ext_version != NULL) {
    register_ext_version(ext_version);
  }
  return (errors == 0);
}

/* ---------------------------- */

/*********************/
/* UTILITY FUNCTIONS */
/*********************/

String alloc_string(String str, size_t size) {
  /*
  * Allocates $size bytes for the String $str and returns a pointer
  * to the allocate memory (or NULL if something went wrong).
  * $str must be either NULL or a pointer returned from a
  * previously call of (c|m|re)alloc.
  */
    dprint("(re/alloc) size=%zu\n", size);
    str = realloc(str, sizeof(String) * size);
    return str;
}

String path_join(String first, String last) {
  /*
   * Joins the two path component $first and $last in a
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
  if (strncmp(& first[strlen(first)-1], _PATHSEP, 1))
    strncat(joined, _PATHSEP, strlen(_PATHSEP));
  strncat(joined, last, strlen(last));
  return joined;
}

int check_path(String path, int mask) {
  /*
   * Loosly check if a file/dir exists and is readable and writable.
   * see <man 2 access> for details.
   * $mask can be R_OK, W_OK, X_OK or a bitwise of them.
   */
  return access(path, mask);
}

String get_current_dir(String dest, size_t size) {
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

static awk_value_t * do_check_path(int nargs, awk_value_t *result, __attribute__((unused)) struct awk_ext_func *finfo) {
  /*
   * Loosely checks if a file/dir exists and is readable *and* writable.
   * Return true if success, 0 otherwise (non-existent path, no permissions, etc.).
   * see <man 2 access> for details.
   * Accetps one or two arguments. The first must a path to the file/dir to check,
   * the (optional) second argument must a string representing the file mode
   * (readable, writable, ...), written as a combination of "r" "w" "x".
   * Default to "r" only.
   */
  awk_value_t path;
  awk_value_t mask_s;
  size_t i, mask = 0;
  assert(result != NULL);
  make_number(0, result);

  if (nargs > 2) {
    eprint("too many arguments\n");
    goto out;
  }
  if (! get_argument(0, AWK_STRING, & path)) {
    eprint("can't retrieve path\n");
    goto out;
  }
  if (nargs == 2) {
      if (! get_argument(1, AWK_STRING, & mask_s)) {
	eprint("can't retrieve mask\n");
	goto out;
      }
      for (i=0; i<mask_s.str_value.len; i++) {
	switch (mask_s.str_value.str[i]) {
	case 'r': mask |= R_OK; break;
	case 'w': mask |= W_OK; break;
	case 'x': mask |= X_OK; break;
	default:
	  eprint("Unknown mask value <%c>\n", mask_s.str_value.str[i]);
	  goto out;
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

static awk_value_t * do_getcwd(int nargs, awk_value_t *result, __attribute__((unused)) struct awk_ext_func *finfo) {
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

  if (nargs > 0) {
    eprint("too many arguments\n");
    goto out;
  }
  if (NULL == (current_dir = get_current_dir(current_dir, dir_size)))
    goto out;
  
  erealloc(pathname, String, strlen(current_dir)+1, __func__);
  strcpy(pathname, current_dir);
  make_malloced_string(pathname, strlen(pathname), result);

 out:
  free(current_dir);
  return result;
}

static awk_value_t * do_mktemp(int nargs, awk_value_t *result, __attribute__((unused)) struct awk_ext_func *finfo) {
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

  if (NULL == (currdir = get_current_dir(currdir, currdirsize)))
    goto out;
  cwd = currdir;
  
  if (nargs > 1) {
    eprint("too many arguments\n");
    goto out;
  } else if (nargs == 1) {
    if (! get_argument(0, AWK_STRING, & tmp_dir)) {
      eprint("can't retrieve dir\n");
      goto out;
    } 
    cwd = tmp_dir.str_value.str;
  }

  if (NULL == (fullpath = path_join(cwd, template)))
    goto out;
  
  if (-1 == (fd = mkstemp(fullpath))) {
    eprint("mkstemp failed: %s", strerror(errno));
    goto out;
  } else {
    if (-1 == close(fd)) {
      eprint("close failed: %s", strerror(errno));
    }
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

static awk_value_t * do_rm(int nargs, awk_value_t *result, __attribute__((unused)) struct awk_ext_func *finfo) {
  /*
  * Removes the $nargs[0] file (or directory, if epmty).
  * using the remove system call.
  * See <man 3 remove> for details.
  */
  assert(result != NULL);
  make_number(0.0, result);
  awk_value_t pathname;
  if (nargs != 1) {
    eprint("too many arguments! One expected: path_to_file_or_dir\n");
    goto out;
  }

  if (! get_argument(0, AWK_STRING, & pathname)) {
    if (pathname.val_type != AWK_STRING)
      eprint("wrong type argument: <%s> (expected: <%s>)\n", _val_types[pathname.val_type], name_to_string(AWK_STRING));
    else
      eprint("can't get path <%s>\n", pathname.str_value.str);
    goto out;
  }

  if (-1 == remove(pathname.str_value.str)) {
    eprint("<%s> %s\n", pathname.str_value.str, strerror(errno));
    goto out;
  }
  
  make_number(1, result); 
 out:
  return result;
}

/* COMPILE WITH:
gcc -fPIC -shared -DHAVE_CONFIG_H -c -O -g -I/usr/include -I~/local/include/awk -Wall -Wextra sysutils.c && gcc -o sysutils.so -shared sysutils.o && cp sysutils.so ~/local/lib/awk/
*/
