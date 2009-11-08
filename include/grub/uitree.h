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

#ifndef GRUB_UITREE_HEADER
#define GRUB_UITREE_HEADER 1

#include <grub/err.h>
#include <grub/tree.h>

struct grub_uiprop
{
  struct grub_uiprop *next;
  char *value;
  char name[0];
};
typedef struct grub_uiprop *grub_uiprop_t;

struct grub_uitree
{
  struct grub_uitree *parent;
  struct grub_uitree *child;
  struct grub_uitree *next;
  struct grub_uiprop *prop;
  void *data;
  int flags;
  char name[0];
};
typedef struct grub_uitree *grub_uitree_t;

extern struct grub_uitree grub_uitree_root;

#define GRUB_UITREE_LOAD_METHOD_SINGLE	0
#define GRUB_UITREE_LOAD_METHOD_APPEND	1
#define GRUB_UITREE_LOAD_METHOD_REPLACE	2
#define GRUB_UITREE_LOAD_METHOD_MERGE	3

void grub_uitree_dump (grub_uitree_t node);
grub_uitree_t grub_uitree_find (grub_uitree_t node, const char *name);
grub_uitree_t grub_uitree_find_id (grub_uitree_t node, const char *name);
grub_uitree_t grub_uitree_create_node (const char *name);
grub_uitree_t grub_uitree_load (char *buf, int size, int *ret);
grub_uitree_t grub_uitree_load_string (grub_uitree_t root, char *buf,
				       int method);
grub_uitree_t grub_uitree_load_file (grub_uitree_t root, const char *name,
				     int method);
void grub_uitree_free (grub_uitree_t node);
grub_err_t grub_uitree_set_prop (grub_uitree_t node, const char *name,
				 const char *value);
char *grub_uitree_get_prop (grub_uitree_t node, const char *name);

#endif
