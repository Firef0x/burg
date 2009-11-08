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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>

GRUB_EXPORT(grub_dialog_create);
GRUB_EXPORT(grub_dialog_set_parm);
GRUB_EXPORT(grub_dialog_get_parm);
GRUB_EXPORT(grub_dialog_popup);
GRUB_EXPORT(grub_dialog_free);

static grub_uitree_t
copy_node (grub_uitree_t node)
{
  grub_uitree_t n, child;
  grub_uiprop_t *ptr, prop;

  n = grub_uitree_create_node (node->name);
  if (! n)
    return 0;

  ptr = &n->prop;
  prop = node->prop;
  while (prop)
    {
      grub_uiprop_t p;
      int len;

      len = (prop->value - (char *) prop) + grub_strlen (prop->value) + 1;
      p = grub_malloc (len);
      if (! p)
	{
	  grub_uitree_free (n);
	  return 0;
	}
      grub_memcpy (p, prop, len);
      *ptr = p;
      ptr = &(p->next);
      prop = prop->next;
    }

  child = node->child;
  while (child)
    {
      grub_uitree_t c;

      c = copy_node (child);
      if (! c)
	{
	  grub_uitree_free (n);
	  return 0;
	}
      grub_tree_add_child (GRUB_AS_TREE (n), GRUB_AS_TREE (c), -1);
      child = child->next;
    }

  return n;
}

grub_uitree_t
grub_dialog_create (const char *name, int copy, int index,
		    grub_uitree_t *menu, grub_uitree_t *save)
{
  grub_uitree_t node;
  grub_uitree_t parent;

  parent = grub_uitree_find (&grub_uitree_root, name);
  if (! parent)
    return 0;

  node = parent->child;
  while ((index) && (node))
    {
      index--;
      node = node->next;
    }

  if (! node)
    return 0;

  if (copy)
    node = copy_node (node);
  else
    {
      *menu = parent;
      *save = parent->child;
      parent->child = node->next;
    }

  return node;
}

static grub_uitree_t
find_prop (grub_uitree_t node, char *path, char **out)
{
  while (1)
    {
      char *n;

      n = grub_menu_next_field (path, '.');
      if (! n)
	{
	  *out = path;
	  return node;
	}

      node = grub_uitree_find (node, path);
      grub_menu_restore_field (n, '.');
      if (! node)
	return 0;
      path = n;
    }
}

static grub_uitree_t
find_parm (grub_uitree_t node, char *parm, char *name,
	   char **out, char **next)
{
  *next = 0;
  *out = name;

  if (grub_strchr (name, '.'))
    node = find_prop (node, name, out);
  else if (parm)
    do
      {
	char *n, *v;

	n = grub_menu_next_field (parm, ':');
	v = grub_menu_next_field (parm, '=');
	if ((v) && (! grub_strcmp (parm, name)))
	  {
	    grub_menu_restore_field (v, '=');
	    *next = n;
	    return find_prop (node, v, out);
	  }
	grub_menu_restore_field (v, '=');
	grub_menu_restore_field (n, ':');
	parm = n;
      } while (parm);

  return node;
}

int
grub_dialog_set_parm (grub_uitree_t node, char *parm,
		      char *name, const char *value)
{
  char *out, *next;
  int result;

  node = find_parm (node, parm, name, &out, &next);
  result = (node) ? (grub_uitree_set_prop (node, out, value) == 0) : 0;
  grub_menu_restore_field (next, ':');

  return result;
}

char *
grub_dialog_get_parm (grub_uitree_t node, char *parm, char *name)
{
  char *out, *next, *result;

  node = find_parm (node, parm, name, &out, &next);
  result = (node) ? grub_uitree_get_prop (node, out) : 0;
  grub_menu_restore_field (next, ':');

  return result;
}

grub_err_t
grub_dialog_popup (grub_uitree_t node)
{
  grub_err_t r;
  grub_uitree_t save;

  grub_widget_create (node);
  grub_widget_init (node);
  node->flags |= GRUB_WIDGET_FLAG_FIXED_XY;
  save = grub_widget_current_node;
  r = grub_widget_input (node, 1);
  grub_widget_current_node = save;
  grub_widget_free (node);

  return r;
}

void
grub_dialog_free (grub_uitree_t node, grub_uitree_t menu, grub_uitree_t save)
{
  grub_tree_remove_node (GRUB_AS_TREE (node));
  if (save)
    {
      node->next = menu->child;
      node->parent = menu;
      menu->child = save;
    }
  else
    grub_uitree_free (node);
}
