/* grub-script-check.c - check grub script file for syntax errors */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/util/misc.h>
#include <grub/i18n.h>
#include <grub/parser.h>
#include <grub/script_sh.h>

#include <grub_script_check_init.h>

#define _GNU_SOURCE	1

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <getline.h>

#include "progname.h"

void
grub_putchar (int c)
{
  putchar (c);
}

int
grub_getkey (void)
{
  return -1;
}

void
grub_refresh (void)
{
  fflush (stdout);
}

char *
grub_script_execute_argument_to_string (struct grub_script_arg *arg __attribute__ ((unused)))
{
  return 0;
}

grub_err_t
grub_script_execute_cmdline (struct grub_script_cmd *cmd __attribute__ ((unused)))
{
  return 0;
}

grub_err_t
grub_script_execute_cmdblock (struct grub_script_cmd *cmd __attribute__ ((unused)))
{
  return 0;
}

grub_err_t
grub_script_execute_cmdif (struct grub_script_cmd *cmd __attribute__ ((unused)))
{
  return 0;
}

grub_err_t
grub_script_execute_menuentry (struct grub_script_cmd *cmd __attribute__ ((unused)))
{
  return 0;
}

grub_err_t
grub_script_execute (struct grub_script *script)
{
  if (script == 0 || script->cmd == 0)
    return 0;

  return script->cmd->exec (script->cmd);
}

static struct option options[] =
  {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr,
	     "Try ``%s --help'' for more information.\n", program_name);
  else
    printf ("\
Usage: %s [PATH]\n\
\n\
Checks GRUB script configuration file for syntax errors.\n\
\n\
  -h, --help                display this message and exit\n\
  -V, --version             print version information and exit\n\
  -v, --verbose             print script being processed\n\
\n\
Report bugs to <%s>.\n\
", program_name,
	    PACKAGE_BUGREPORT);
  exit (status);
}

struct main_closure
{
  FILE *file;
  int verbose;
};

static grub_err_t
get_config_line (char **line, int cont __attribute__ ((unused)), void *closure)
{
  struct main_closure *c = closure;
  int i;
  char *cmdline = 0;
  size_t len = 0;
  ssize_t read;

  read = getline(&cmdline, &len, (c->file ?: stdin));
  if (read == -1)
    {
      *line = 0;
      grub_errno = GRUB_ERR_READ_ERROR;

      if (cmdline)
	free (cmdline);
      return grub_errno;
    }

  if (c->verbose)
    grub_printf("%s", cmdline);

  for (i = 0; cmdline[i] != '\0'; i++)
    {
      /* Replace tabs and carriage returns with spaces.  */
      if (cmdline[i] == '\t' || cmdline[i] == '\r')
	cmdline[i] = ' ';

      /* Replace '\n' with '\0'.  */
      if (cmdline[i] == '\n')
	cmdline[i] = '\0';
      }

  *line = grub_strdup (cmdline);

  free (cmdline);
  return 0;
}

int
main (int argc, char *argv[])
{
  char *argument;
  char *input;
  struct grub_script *script = 0;
  struct main_closure c;

  c.file = 0;
  c.verbose = 0;

  set_program_name (argv[0]);
  grub_util_init_nls ();

  /* Check for options.  */
  while (1)
    {
      int ch = getopt_long (argc, argv, "hvV", options, 0);

      if (ch == -1)
	break;
      else
	switch (ch)
	  {
	  case 'h':
	    usage (0);
	    break;

	  case 'V':
	    printf ("%s (%s) %s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    c.verbose = 1;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  /* Obtain ARGUMENT.  */
  if (optind >= argc)
    {
      c.file = 0; /* read from stdin */
    }
  else if (optind + 1 != argc)
    {
      fprintf (stderr, "Unknown extra argument `%s'.\n", argv[optind + 1]);
      usage (1);
    }
  else
    {
      argument = argv[optind];
      c.file = fopen (argument, "r");
      if (! c.file)
	{
	  fprintf (stderr, "%s: %s: %s\n", program_name, argument, strerror(errno));
	  usage (1);
	}
    }

  /* Initialize all modules.  */
  grub_init_all ();

  do
    {
      input = 0;
      get_config_line(&input, 0, &c);
      if (! input)
	break;

      script = grub_script_parse (input, get_config_line, &c);
      if (script)
	{
	  grub_script_execute (script);
	  grub_script_free (script);
	}

      grub_free (input);
    } while (script != 0);

  /* Free resources.  */
  grub_fini_all ();
  if (c.file) fclose (c.file);

  return (script == 0);
}
