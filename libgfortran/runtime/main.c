/* Copyright (C) 2002-2015 Free Software Foundation, Inc.
   Contributed by Andy Vaught and Paul Brook <paul@nowt.org>

This file is part of the GNU Fortran runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "libgfortran.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Stupid function to be sure the constructor is always linked in, even
   in the case of static linking.  See PR libfortran/22298 for details.  */
void
stupid_function_name_for_static_linking (void)
{
  return;
}

/* This will be 0 for little-endian
   machines and 1 for big-endian machines.  */
int big_endian = 0;


/* Figure out endianness for this machine.  */

static void
determine_endianness (void)
{
  union
  {
    GFC_LOGICAL_8 l8;
    GFC_LOGICAL_4 l4[2];
  } u;

  u.l8 = 1;
  if (u.l4[0])
    big_endian = 0;
  else if (u.l4[1])
    big_endian = 1;
  else
    runtime_error ("Unable to determine machine endianness");
}


static int argc_save;
static char **argv_save;

static const char *exe_path;
static bool please_free_exe_path_when_done;

/* Save the path under which the program was called, for use in the
   backtrace routines.  */
void
store_exe_path (const char * argv0)
{
#ifndef DIR_SEPARATOR   
#define DIR_SEPARATOR '/'
#endif

  char *cwd, *path;

  /* This can only happen if store_exe_path is called multiple times.  */
  if (please_free_exe_path_when_done)
    free ((char *) exe_path);

  /* Reading the /proc/self/exe symlink is Linux-specific(?), but if
     it works it gives the correct answer.  */
#ifdef HAVE_READLINK
  ssize_t len, psize = 256;
  while (1)
    {
      path = xmalloc (psize);
      len = readlink ("/proc/self/exe", path, psize);
      if (len < 0)
	{
	  free (path);
	  break;
	}
      else if (len < psize)
	{
	  path[len] = '\0';
	  exe_path = strdup (path);
	  free (path);
	  please_free_exe_path_when_done = true;
	  return;
	}
      /* The remaining option is len == psize.  */
      free (path);
      psize *= 4;
    }
#endif

  /* If the path is absolute or on a simulator where argv is not set.  */
#ifdef __MINGW32__
  if (argv0 == NULL
      || ('A' <= argv0[0] && argv0[0] <= 'Z' && argv0[1] == ':')
      || ('a' <= argv0[0] && argv0[0] <= 'z' && argv0[1] == ':')
      || (argv0[0] == '/' && argv0[1] == '/')
      || (argv0[0] == '\\' && argv0[1] == '\\'))
#else
  if (argv0 == NULL || argv0[0] == DIR_SEPARATOR)
#endif
    {
      exe_path = argv0;
      please_free_exe_path_when_done = false;
      return;
    }

#ifdef HAVE_GETCWD
  size_t cwdsize = 256;
  while (1)
    {
      cwd = xmalloc (cwdsize);
      if (getcwd (cwd, cwdsize))
	break;
      else if (errno == ERANGE)
	{
	  free (cwd);
	  cwdsize *= 4;
	}
      else
	{
	  free (cwd);
	  cwd = NULL;
	  break;
	}
    }
#else
  cwd = NULL;
#endif

  if (!cwd)
    {
      exe_path = argv0;
      please_free_exe_path_when_done = false;
      return;
    }

  /* exe_path will be cwd + "/" + argv[0] + "\0".  This will not work
     if the executable is not in the cwd, but at this point we're out
     of better ideas.  */
  size_t pathlen = strlen (cwd) + 1 + strlen (argv0) + 1;
  path = xmalloc (pathlen);
  snprintf (path, pathlen, "%s%c%s", cwd, DIR_SEPARATOR, argv0);
  free (cwd);
  exe_path = path;
  please_free_exe_path_when_done = true;
}


/* Return the full path of the executable.  */
char *
full_exe_path (void)
{
  return (char *) exe_path;
}


#ifndef HAVE_STRTOK_R
static char*
gfstrtok_r (char *str, const char *delim, 
	    char **saveptr __attribute__ ((unused)))
{
  return strtok (str, delim);
}
#define strtok_r gfstrtok_r
#endif

char *addr2line_path;

/* Find addr2line and store the path.  */

void
find_addr2line (void)
{
#ifdef HAVE_ACCESS
#define A2L_LEN 11
  char *path = secure_getenv ("PATH");
  if (!path)
    return;
  char *tp = strdup (path);
  if (!tp)
    return;
  size_t n = strlen (path);
  char *ap = xmalloc (n + A2L_LEN);
  char *saveptr;
  for (char *str = tp;; str = NULL)
    {
      char *token = strtok_r (str, ":", &saveptr);
      if (!token)
	break;
      size_t toklen = strlen (token);
      memcpy (ap, token, toklen);
      memcpy (ap + toklen, "/addr2line", A2L_LEN);
      if (access (ap, R_OK|X_OK) == 0)
	{
	  addr2line_path = strdup (ap);
	  break;
	}
    }
  free (tp);
  free (ap);
#endif
}


/* Set the saved values of the command line arguments.  */

void
set_args (int argc, char **argv)
{
  argc_save = argc;
  argv_save = argv;
  store_exe_path (argv[0]);
}
iexport(set_args);


/* Retrieve the saved values of the command line arguments.  */

void
get_args (int *argc, char ***argv)
{
  *argc = argc_save;
  *argv = argv_save;
}


/* Initialize the runtime library.  */

static void __attribute__((constructor))
init (void)
{
  /* Figure out the machine endianness.  */
  determine_endianness ();

  /* Must be first */
  init_variables ();

  init_units ();

  /* If (and only if) the user asked for it, set up the FPU state.  */
  if (options.fpe != 0)
    set_fpu ();

  init_compile_options ();

#ifdef DEBUG
  /* Check for special command lines.  */

  if (argc > 1 && strcmp (argv[1], "--help") == 0)
    show_variables ();

  /* if (argc > 1 && strcmp(argv[1], "--resume") == 0) resume();  */
#endif

  if (options.backtrace == 1)
    find_addr2line ();

  random_seed_i4 (NULL, NULL, NULL);
}


/* Cleanup the runtime library.  */

static void __attribute__((destructor))
cleanup (void)
{
  close_units ();
  
  if (please_free_exe_path_when_done)
    free ((char *) exe_path);

  free (addr2line_path);
}
