/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/loader.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/video.h>
#include <grub/i386/relocator16.h>
#include <grub/machine/biosnum.h>

static grub_dl_t my_mod;

static char *relocator;
static grub_uint32_t dest;
static struct grub_relocator32_state state;

static grub_err_t
grub_loadbin_unload (void)
{
  grub_dl_unref (my_mod);
  grub_relocator16_free (relocator);
  relocator = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_loadbin_boot (void)
{
  grub_video_set_mode ("text", 0, 0);
  return grub_relocator16_boot (relocator, dest, state);
}

static int
get_drive_number (void)
{
  int drive, part;
  char *p;

  drive = grub_get_root_biosnumber ();
  part = -1;
  p = grub_env_get ("root");
  if (p)
    {
      p = grub_strchr (p, ',');
      if (p)
	part = grub_strtoul (p + 1, 0, 0) - 1;
    }

  return drive | ((part & 0xff) << 8);
}

static grub_err_t
grub_cmd_loadbin (grub_command_t cmd, int argc, char *argv[])
{
  grub_file_t file = 0;
  grub_size_t size;

  grub_dl_ref (my_mod);
  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, "no kernel specified");
      goto fail;
    }

  file = grub_file_open (argv[0]);
  if (! file)
    goto fail;

  size = grub_file_size (file);
  if (size == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, "file is empty");
      goto fail;
    }

  grub_relocator16_free (relocator);
  relocator = grub_relocator16_alloc (size);
  if (grub_errno)
    goto fail;

  if (grub_file_read (file, relocator, size) != (int) size)
    {
      grub_error (GRUB_ERR_READ_ERROR, "file read fails");
      goto fail;
    }

  if (cmd->name[0] == 'n')
    {
      dest = 0x20000;
      state.esp = 0x7000;
      state.eip = 0x20000000;
      state.edx = get_drive_number ();
    }
  else if (cmd->name[0] == 'f')
    {
      dest = 0x600;
      state.esp = 0x400;
      state.eip = 0x600000;
      state.ebx = get_drive_number ();
    }

  if (grub_errno == GRUB_ERR_NONE)
    grub_loader_set (grub_loadbin_boot, grub_loadbin_unload, 1);

 fail:

  if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    grub_loadbin_unload ();

  return grub_errno;
}

static grub_command_t cmd_ntldr, cmd_freedos;

GRUB_MOD_INIT(loadbin)
{
  cmd_ntldr =
    grub_register_command ("ntldr", grub_cmd_loadbin,
			   0, N_("Load ntldr/grldr."));
  cmd_freedos =
    grub_register_command ("freedos", grub_cmd_loadbin,
			   0, N_("Load kernel.sys."));

  my_mod = mod;
}

GRUB_MOD_FINI(loadbin)
{
  grub_unregister_command (cmd_ntldr);
}
