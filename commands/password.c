/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/auth.h>
#include <grub/list.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/dl.h>
#include <grub/lib.h>
#include <grub/command.h>
#include <grub/i18n.h>

#define PASSWORD_PLAIN	1
#define PASSWORD_MD5	2

static grub_dl_t my_mod;

static int
check_password (const char *user __attribute__ ((unused)),
		const char *entered, void *data)
{
  char *password = data;
  
  if (! entered)
    entered = "";

  if (! password)
    return 0;
  
  switch (password[0])
    {
    case PASSWORD_PLAIN:
      return (grub_auth_strcmp (entered, password + 1) == 0);

    case PASSWORD_MD5:
      return (grub_check_md5_password (entered, password + 1) == 0);
      
    default:
      return 0;
    }
}

static grub_err_t
grub_cmd_password (grub_command_t cmd __attribute__ ((unused)),
		   int argc, char **args)
{
  grub_err_t err;
  char *pass;
  int type;

  if ((argc > 0) && (! grub_strcmp (args[0], "--md5")))
    {
      type = PASSWORD_MD5;
      argc--;
      args++;
    }
  else
    type = PASSWORD_PLAIN;
    
  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "two arguments expected");

  pass = grub_malloc (grub_strlen (args[1]) + 2);
  if (!pass)
    return grub_errno;

  pass[0] = type;
  grub_strcpy (pass + 1, args[1]);  

  err = grub_auth_register_authentication (args[0], check_password, pass);
  if (err)
    {
      grub_free (pass);
      return err;
    }
  grub_dl_ref (my_mod);
  return GRUB_ERR_NONE;
}

static grub_command_t cmd;

GRUB_MOD_INIT(password)
{
  my_mod = mod;
  cmd = grub_register_command ("password", grub_cmd_password,
			       N_("[--md5] USER PASSWORD"),
			       N_("Set user password (plaintext). "
			       "Unrecommended and insecure."));
}

GRUB_MOD_FINI(password)
{
  grub_unregister_command (cmd);
}
