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

#define __module__ "sysutils"
#define eprint(fmt, ...) fprintf(stderr, __msg_prologue fmt, __module__, __func__, ##__VA_ARGS__)
#define _DEBUGLVL 1
#if (_DEBUGLVL)
#define dprint eprint
#define __msg_prologue "Debug: %s @%s: "
#else
#define __msg_prologue "Error: %s @%s: "
#define dprint(fmt, ...) do {} while (0)
#endif

typedef char * String;

static awk_value_t * do_rm(int nargs, awk_value_t *result, struct awk_ext_func *finfo);
static awk_value_t * do_mktemp(int nargs, awk_value_t *result, struct awk_ext_func *finfo);

/* ----- boilerplate code ----- */
int plugin_is_GPL_compatible;
static const gawk_api_t *api;

static awk_ext_id_t ext_id;
static const char *ext_version = "0.1";

static awk_ext_func_t func_table[] = {
  { "rm", do_rm, 1, 1, awk_false, NULL },
  { "mktemp", do_mktemp, 1, 0, awk_false, NULL },
};

static awk_bool_t (*init_func)(void) = NULL;


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
//XXX: add getcwd ext func

String alloc_string(String str, size_t new_size, int re_alloc) {
  if (re_alloc) {
    dprint("(malloc) size=%zu\n", new_size);
    if (NULL == (str = malloc(sizeof(String) * new_size))) {
      return NULL;
    } else
      return str;
  } else {
    dprint("(realloc) new_size=%zu\n", new_size);
    if (NULL == (str = realloc(str, sizeof(str) * new_size))) {
      return NULL;
    } else
      return str;
  }
}

String get_current_dir(String dest, size_t size) {
  while (1) {
    if (NULL == (dest = getcwd(dest, size))) {
      if (errno == ERANGE) {
	size *= 2;
	if (NULL == (dest = alloc_string(dest, size, 1))) {
	  eprint("%s\n", strerror(errno));
	  return NULL;
	}
      } else {
	eprint("%s\n", strerror(errno));
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

static awk_value_t * do_mktemp(int nargs, awk_value_t *result, struct awk_ext_func *finfo) {
  String pathname = NULL;
  String currdir = NULL;
  String cwd = NULL;
  size_t currdirsize = 100;
  awk_value_t tmp_dir;
  int fd;
  char template[] = "tmp_XXXXXX";
  mode_t prev_umask = umask(S_IWGRP | S_IWOTH);

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
    if (-1 == chdir(tmp_dir.str_value.str)) {
      eprint("%s <%s>\n", strerror(errno), tmp_dir.str_value.str);
      goto out;
    }
    cwd = tmp_dir.str_value.str;
  }

  dprint("cwd is <%s>\n", cwd);

  if (-1 == (fd = mkstemp(template))) {
    eprint("mkstemp failed: %s\n", strerror(errno));
    goto out;
  } else {
    if (-1 == close(fd)) {
      eprint("close failed: %s\n", strerror(errno));
    }
  }
  dprint("create temp file <%s> in dir <%s>\n", template, cwd);
  //XXX+TODO: join path ?? (cwd, pathname
  emalloc(pathname, String, sizeof(template), __func__);
  strcpy(pathname, template);
  make_malloced_string(pathname, strlen(pathname), result);

  if (nargs == 1) {
    if (-1 == chdir(currdir)) {
      eprint("%s <%s>\n", strerror(errno), currdir);
      goto out;
    }
    cwd = currdir;
  }
  dprint("cwd is <%s>\n", cwd);
  
 out:
  free(currdir);
  return result;

}

static awk_value_t * do_rm(int nargs, awk_value_t *result, struct awk_ext_func *finfo) {
  /*
  * Removes the $nargs[0] file or directory (if epmty)
  * using the remove system call.
  * See <man 3 remove> for details.
  */
  assert(result != NULL);
  make_number(0.0, result);
  awk_value_t pathname;
  if (nargs != 1) {
    eprint("one argument expected: path_to_file_or_dir\n");
    goto out;
  }
  if (! get_argument(0, AWK_STRING, & pathname)) {
    if (pathname.val_type != AWK_STRING)
      eprint("wrong type argument: <%d> (expected: <%d>)\n", pathname.val_type, AWK_STRING); //XXX+TODO: ++meaningful
    else
      eprint("can't get path <%s>\n", pathname.str_value.str);
    goto out;
  }

  if (-1 == remove(pathname.str_value.str)) {
    eprint("%s <%s>\n", strerror(errno), pathname.str_value.str);
    goto out;
  }
  
  make_number(1, result); 
 out:
  return result;
}

/* COMPILE WITH:
gcc -fPIC -shared -DHAVE_CONFIG_H -c -O -g -I/usr/include -Wall -Wextra sysutils.c && gcc -o sysutils.so -shared sysutils.o && cp sysutils.so ~/local/lib/awk/
*/
