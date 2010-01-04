/* main.c - the normal mode main routine */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/kernel.h>
#include <grub/normal.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/lib.h>
#include <grub/env.h>
#include <grub/menu.h>
#include <grub/command.h>
#include <grub/i18n.h>

/* This starts the normal mode.  */
static void
grub_enter_normal_mode (const char *config)
{
  read_command_list ();
  read_fs_list ();
  read_handler_list ();
  grub_autolist_font = grub_autolist_load ("fonts/font.lst");
  grub_errno = 0;
  grub_command_execute ("parser.grub", 0, 0);
  grub_command_execute ("menu_viewer.normal", 0, 0);
  grub_menu_execute (config, 0, 0);
}

/* Enter normal mode from rescue mode.  */
static grub_err_t
grub_cmd_normal (struct grub_command *cmd,
		 int argc, char *argv[])
{
  grub_unregister_command (cmd);

  if (argc == 0)
    {
      /* Guess the config filename. It is necessary to make CONFIG static,
	 so that it won't get broken by longjmp.  */
      static char *config;
      const char *prefix;

      prefix = grub_env_get ("prefix");
      if (prefix)
	{
	  config = grub_malloc (grub_strlen (prefix) + sizeof ("/grub.cfg"));
	  if (! config)
	    goto quit;

	  grub_sprintf (config, "%s/grub.cfg", prefix);
	  grub_enter_normal_mode (config);
	  grub_free (config);
	}
      else
	grub_enter_normal_mode (0);
    }
  else
    grub_enter_normal_mode (argv[0]);

quit:
  return 0;
}

GRUB_MOD_INIT(normal)
{
  /* Normal mode shouldn't be unloaded.  */
  if (mod)
    grub_dl_ref (mod);

  grub_history_init (GRUB_DEFAULT_HISTORY_SIZE);

  /* Register a command "normal" for the rescue mode.  */
  grub_reg_cmd ("normal", grub_cmd_normal, 0, "Enter normal mode.", 0);
}

GRUB_MOD_FINI(normal)
{
  grub_history_init (0);
  grub_fs_autoload_hook = 0;
  free_handler_list ();
}
