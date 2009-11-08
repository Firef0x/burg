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

#ifndef GRUB_DIALOG_HEADER
#define GRUB_DIALOG_HEADER 1

#include <grub/uitree.h>

grub_uitree_t grub_dialog_create (const char *name, int copy, int index,
				  grub_uitree_t *menu, grub_uitree_t *save);
int grub_dialog_set_parm (grub_uitree_t node, char *parm, char *name,
			  const char *value);
char *grub_dialog_get_parm (grub_uitree_t node, char *parm, char *name);
grub_err_t grub_dialog_popup (grub_uitree_t node);
void grub_dialog_free (grub_uitree_t node, grub_uitree_t menu,
		       grub_uitree_t save);

#endif
