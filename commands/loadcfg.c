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

#include <grub/dl.h>
#include <grub/command.h>
#include <grub/uitree.h>

static grub_err_t
grub_cmd_dump (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  grub_uitree_t root;

  root = &grub_uitree_root;
  if (argc)
    {
      root = grub_uitree_find (root, args[0]);
      if (! root)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, "section not found");
    }

  grub_uitree_dump (root);

  return grub_errno;
}

static grub_err_t
grub_cmd_loadcfg (grub_command_t cmd, int argc, char **args)
{
  int method = (cmd->name[0] == 'l') ? GRUB_UITREE_LOAD_METHOD_REPLACE :
    GRUB_UITREE_LOAD_METHOD_MERGE;

  while (argc)
    {
      grub_uitree_load_file (&grub_uitree_root, *args, method);
      argc--;
      args++;
    }

  return grub_errno;
}

static grub_command_t cmd_dump, cmd_load, cmd_merge;

GRUB_MOD_INIT(loadcfg)
{
  cmd_dump =
    grub_register_command ("dump_config", grub_cmd_dump,
			   "dump_config [NAME]", "Dump config section.");
  cmd_load =
    grub_register_command ("load_config", grub_cmd_loadcfg,
			   "load_config [FILES]", "Load config file.");
  cmd_merge =
    grub_register_command ("merge_config", grub_cmd_loadcfg,
			   "merge_config [FILES]", "Merge config file.");
}

GRUB_MOD_FINI(loadcfg)
{
  grub_unregister_command (cmd_dump);
  grub_unregister_command (cmd_load);
  grub_unregister_command (cmd_merge);
}
