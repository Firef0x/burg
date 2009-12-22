/* memrw.c - command to read / write physical memory  */
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
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/env.h>
#include <grub/cpu/io.h>

static grub_extcmd_t cmd_read_byte, cmd_read_word, cmd_read_dword;
static grub_command_t cmd_write_byte, cmd_write_word, cmd_write_dword;

static const struct grub_arg_option options[] =
  {
    {0, 'v', 0, "Save read value into variable VARNAME.",
     "VARNAME", ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };


static grub_err_t
grub_cmd_read (grub_extcmd_t cmd, int argc, char **argv)
{
  grub_target_addr_t addr;
  grub_uint32_t value = 0;
  char buf[sizeof ("XXXXXXXX")];

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "Invalid number of arguments");

  addr = grub_strtoul (argv[0], 0, 0);
  switch (cmd->cmd->name[sizeof ("grub_in") - 1])
    {
    case 'l':
      value = grub_inl (addr);
      break;

    case 'w':
      value = grub_inw (addr);
      break;

    case 'b':
      value = grub_inb (addr);
      break;
    }

  if (cmd->state[0].set)
    {
      grub_sprintf (buf, "%x", value);
      grub_env_set (cmd->state[0].arg, buf);
    }
  else
    grub_printf ("0x%x\n", value);

  return 0;
}

static grub_err_t
grub_cmd_write (grub_command_t cmd, int argc, char **argv)
{
  grub_target_addr_t addr;
  grub_uint32_t value;
  grub_uint32_t mask = 0xffffffff;

  if (argc != 2 && argc != 3)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "Invalid number of arguments");

  addr = grub_strtoul (argv[0], 0, 0);
  value = grub_strtoul (argv[1], 0, 0);
  if (argc == 3)
    mask = grub_strtoul (argv[2], 0, 0);
  value &= mask;
  switch (cmd->name[sizeof ("grub_out") - 1])
    {
    case 'l':
      if (mask != 0xffffffff)
	grub_outl ((grub_inl (addr) & ~mask) | value, addr);
      else
	grub_outl (value, addr);
      break;

    case 'w':
      if ((mask & 0xffff) != 0xffff)
	grub_outw ((grub_inw (addr) & ~mask) | value, addr);
      else
	grub_outw (value, addr);
      break;

    case 'b':
      if ((mask & 0xff) != 0xff)
	grub_outb ((grub_inb (addr) & ~mask) | value, addr);
      else
	grub_outb (value, addr);
      break;
    }

  return 0;
}

GRUB_MOD_INIT(memrw)
{
  cmd_read_byte =
    grub_register_extcmd ("grub_inb", grub_cmd_read, GRUB_COMMAND_FLAG_BOTH,
			  "grub_inb PORT", "Read byte from PORT.", options);
  cmd_read_word =
    grub_register_extcmd ("grub_inw", grub_cmd_read, GRUB_COMMAND_FLAG_BOTH,
			  "grub_inw PORT", "Read word from PORT.", options);
  cmd_read_dword =
    grub_register_extcmd ("grub_inl", grub_cmd_read, GRUB_COMMAND_FLAG_BOTH,
			  "grub_inl PORT", "Read dword from PORT.", options);
  cmd_write_byte =
    grub_register_command ("grub_outb", grub_cmd_write,
			   "grub_outb PORT VALUE [MASK]", "Write byte VALUE to PORT.");
  cmd_write_word =
    grub_register_command ("grub_outw", grub_cmd_write,
			   "grub_outw PORT VALUE [MASK]", "Write word VALUE to PORT.");
  cmd_write_dword =
    grub_register_command ("grub_outl", grub_cmd_write,
			   "grub_outl ADDR VALUE [MASK]", "Write dword VALUE to PORT.");
}

GRUB_MOD_FINI(memrw)
{
  grub_unregister_extcmd (cmd_read_byte);
  grub_unregister_extcmd (cmd_read_word);
  grub_unregister_extcmd (cmd_read_dword);
  grub_unregister_command (cmd_write_byte);
  grub_unregister_command (cmd_write_word);
  grub_unregister_command (cmd_write_dword);
}
