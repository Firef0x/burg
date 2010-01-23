/* normal.h - prototypes for the normal mode */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_NORMAL_HEADER
#define GRUB_NORMAL_HEADER	1

extern int grub_normal_exit_level;

/* Defined in `handler.c'.  */
void read_handler_list (void);
void free_handler_list (void);

/* Defined in `dyncmd.c'.  */
void read_command_list (void);

/* Defined in `autofs.c'.  */
void read_fs_list (void);

void read_crypto_list (void);

void read_terminal_list (void);

void grub_set_more (int onoff);

#ifdef GRUB_UTIL
void grub_normal_init (void);
void grub_normal_fini (void);
void grub_hello_init (void);
void grub_hello_fini (void);
void grub_ls_init (void);
void grub_ls_fini (void);
void grub_cat_init (void);
void grub_cat_fini (void);
void grub_boot_init (void);
void grub_boot_fini (void);
void grub_cmp_init (void);
void grub_cmp_fini (void);
void grub_terminal_init (void);
void grub_terminal_fini (void);
void grub_loop_init (void);
void grub_loop_fini (void);
void grub_help_init (void);
void grub_help_fini (void);
void grub_halt_init (void);
void grub_halt_fini (void);
void grub_reboot_init (void);
void grub_reboot_fini (void);
void grub_configfile_init (void);
void grub_configfile_fini (void);
void grub_search_init (void);
void grub_search_fini (void);
void grub_test_init (void);
void grub_test_fini (void);
void grub_blocklist_init (void);
void grub_blocklist_fini (void);
#endif

#endif /* ! GRUB_NORMAL_HEADER */
