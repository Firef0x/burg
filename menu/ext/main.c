/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
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
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/extcmd.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>
#include <grub/menu_region.h>
#include <grub/menu_viewer.h>

static const struct grub_arg_option options[] =
  {
    {"string", 's', 0, "load from string", 0, 0},
    {"file", 'f', 0, "load from file.", 0, 0},
    {"copy", 'c', 0, "copy node.", 0, 0},
    {"index", 'i', 0, "set index", 0, ARG_TYPE_INT},
    {"relative", 'r', 0, "relative to current node.", 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

static grub_uitree_t
get_screen (void)
{
  grub_uitree_t root;

  root = grub_uitree_find (&grub_uitree_root, "screen");
  if (! root)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "menu not initialized");

  return root;
}

static void
set_position (grub_uitree_t root, grub_uitree_t node)
{
  char buf[12], *p, *pos;
  grub_widget_t screen, widget;
  int v;
  int horizontal;
  int right, bottom;
  
  pos = grub_uitree_get_prop (grub_widget_current_node->parent, "popup");
  if (! pos)
    pos = "";
  else if (! grub_strcmp (pos, "abs"))
    return;

  horizontal = 0;
  p = grub_uitree_get_prop (grub_widget_current_node->parent, "direction");
  if ((p) &&
      ((! grub_strcmp (p, "left_to_right")) ||
       (! grub_strcmp (p, "right_to_left"))))
    horizontal = 1;

  screen = root->data;
  widget = grub_widget_current_node->data;

  right = (horizontal) ? -1 :
    (widget->org_x * 2 + widget->width <= screen->width);

  if (! grub_strcmp (pos, "right"))
    right = 1;
  else if (! grub_strcmp (pos, "left"))
    right = 0;

  if (right == 0)
    {
      p = "attach_right";
      v = screen->width - widget->org_x;
    }
  else
    {
      p = "attach_left";
      v = widget->org_x;
      if (right > 0)
	v += widget->width;
    }
  grub_sprintf (buf, "%d/%d", v, v);
  grub_uitree_set_prop (node, p, buf);

  bottom = (! horizontal) ? -1 :
    (widget->org_y * 2 + widget->height <= screen->height);
  
  if (! grub_strcmp (pos, "bottom"))
    bottom = 1;
  else if (! grub_strcmp (pos, "top"))
    bottom = 0;

  if (bottom == 0)
    {
      p = "attach_bottom";
      v = screen->height - widget->org_y;
    }
  else
    {
      p = "attach_top";
      v = widget->org_y;
      if (bottom > 0)
	v += widget->height;
    }
  grub_sprintf (buf, "%d/%d", v, v);
  grub_uitree_set_prop (node, p, buf);
}

static grub_err_t
grub_cmd_popup (grub_extcmd_t cmd, int argc, char **args)
{
  struct grub_arg_list *state = cmd->state;
  grub_uitree_t root, node, menu, save;
  int edit, i, r;
  char *parm, *parm1;

  if (! argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "not enough parameter");

  if (! grub_widget_current_node)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no current node");

  root = get_screen ();
  if (! root)
    return grub_errno;

  menu = save = 0;
  if (state[0].set)
    node = grub_uitree_load_string (root, args[0],
				    GRUB_UITREE_LOAD_METHOD_SINGLE);
  else if (state[1].set)
    node = grub_uitree_load_file (root, args[0],
				  GRUB_UITREE_LOAD_METHOD_SINGLE);
  else
    {
      node = grub_dialog_create (args[0], state[2].set, (state[3].set) ?
				 grub_strtoul (state[3].arg, 0, 0) : 0,
				 &menu, &save);
      if (node)
	grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
    }

  if (! node)
    return grub_errno;

  edit = (cmd->cmd->name[5] == 'e');
  parm = grub_uitree_get_prop (node, "parameters");
  parm1 = grub_uitree_get_prop (grub_widget_current_node, "parameters");

  for (i = 1; i < argc; i++)
    {
      char *p, *s;

      p = grub_menu_next_field (args[i], '=');
      if (! p)
	continue;

      s = (edit) ?
	grub_dialog_get_parm (grub_widget_current_node, parm1, p) : p;
      if (s)
	{
	  if (! grub_dialog_set_parm (node, parm, args[i], s))
	    {
	      grub_menu_restore_field (p, '=');
	      grub_dialog_free (node, menu, save);
	      return grub_error (GRUB_ERR_BAD_ARGUMENT,
				 "can\'t set parameter");
	    }
	}

      grub_menu_restore_field (p, '=');
    }

  if (state[4].set)
    set_position (root, node);

  r = grub_dialog_popup (node);

  if ((r == 0) && (edit))
    for (i = 1; i < argc; i++)
      {
	char *p, *s;

	p = grub_menu_next_field (args[i], '=');
	if (! p)
	  continue;

	s = grub_dialog_get_parm (node, parm, args[i]);
	if (s)
	  grub_dialog_set_parm (grub_widget_current_node, parm1, p, s);
	grub_menu_restore_field (p, '=');
      }

  grub_dialog_free (node, menu, save);
  grub_errno = r;
  return r;
}

static grub_err_t
grub_cmd_refresh (grub_command_t cmd __attribute__ ((unused)),
		  int argc __attribute__ ((unused)),
		  char **args __attribute__ ((unused)))
{
  grub_uitree_t node;

  node = get_screen ();
  if (! node)
    return grub_errno;

  grub_widget_free (node);
  grub_widget_create (node);
  grub_widget_init (node);
  grub_widget_draw (node);

  return grub_errno;
}

static grub_err_t
grub_cmd_toggle_mode (grub_command_t cmd __attribute__ ((unused)),
		      int argc __attribute__ ((unused)),
		      char **args __attribute__ ((unused)))
{
  grub_uitree_t node;
  grub_menu_region_t region;
  char *p;

  node = get_screen ();
  if (! node)
    return grub_errno;

  grub_widget_free (node);
  region = grub_menu_region_get_current ();
  p = ((! region) || (! grub_strcmp (region->name, "gfx"))) ?
    "menu_region.text" : "menu_region.gfx";
  grub_command_execute (p, 0, 0);
  grub_widget_create (node);
  grub_widget_init (node);
  grub_widget_draw (node);

  return grub_errno;
}

static grub_uitree_t
create_node (grub_uitree_t menu, grub_uitree_t tree, int *num)
{
  grub_uitree_t node;
  char *parm, *p;

  node = grub_dialog_create ("template_menuitem", 1, 0, 0, 0);
  if (! node)
    return 0;

  parm = grub_uitree_get_prop (node, "parameters");

  grub_dialog_set_parm (node, parm, "title", menu->name);
  p = grub_uitree_get_prop (menu, "class");
  if (p)
    grub_dialog_set_parm (node, parm, "class", p);

  p = grub_uitree_get_prop (menu, "users");
  if (p)
    grub_dialog_set_parm (node, parm, "users", p);

  p = grub_uitree_get_prop (menu, "save");
  if (p)
    grub_dialog_set_parm (node, parm, "save", p);

  if (menu->child)
    {
      char buf[sizeof ("menu_popup -ri XXXXX menu_tree")];
      grub_uitree_t sub, child;

      sub = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
      if (! sub)
	{
	  grub_uitree_free (node);
	  return 0;
	}

      grub_sprintf (buf, "menu_popup -ri %d menu_tree",
		    *num - menu->parent->flags - 1);
      grub_uitree_set_prop (node, "command", buf);

      grub_tree_add_child (GRUB_AS_TREE (tree), GRUB_AS_TREE (sub), -1);
      child = grub_uitree_find_id (sub, "__child__");
      menu->data = (child) ? child : sub;
      menu->flags = *num;
      (*num)++;
    }
  else
    {
      char *cmd;

      cmd = grub_uitree_get_prop (menu, "command");
      if (cmd)
	grub_uitree_set_prop (node, "command", cmd);
    }

  return node;
}

static void
add_sys_menu (grub_uitree_t node, int index, int default_num)
{
  grub_uitree_t menu, tree;
  int num;

  menu = grub_uitree_find (&grub_uitree_root, "menu");
  if (! menu)
    return;

  tree = grub_uitree_find (&grub_uitree_root, "menu_tree");
  if (tree)
    {
      grub_tree_remove_node (GRUB_AS_TREE (tree));
      grub_uitree_free (tree);
    }
  tree = grub_uitree_create_node ("menu_tree");
  if (! tree)
    return;
  grub_tree_add_child (GRUB_AS_TREE (&grub_uitree_root), GRUB_AS_TREE (tree),
		       -1);

  num = 1;
  menu = menu->child;
  while (menu)
    {
      grub_uitree_t child;
      grub_uitree_t n;
      char buf[8];

      n = create_node (menu, tree, &num);
      if (! n)
	return;

      grub_sprintf (buf, "%d", index);
      grub_uitree_set_prop (n, "index", buf);

      grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (n), -1);
      if (index == default_num)
	grub_widget_select_node (n, 1);

      child = menu->child;
      while (child)
	{
	  grub_uitree_t parent;

	  n = create_node (child, tree, &num);
	  if (! n)
	    return;

	  parent = child->parent->data;
	  if (! parent)
	    {
	      grub_error (GRUB_ERR_BAD_ARGUMENT, "parant shouldn't be null");
	      return;
	    }

	  grub_tree_add_child (GRUB_AS_TREE (parent), GRUB_AS_TREE (n), -1);
	  child = grub_tree_next_node (GRUB_AS_TREE (menu),
				       GRUB_AS_TREE (child));
	}
      menu = menu->next;
      index++;
    }
}

static int
add_user_menu (grub_uitree_t node, grub_menu_t menu, int default_num)
{
  grub_menu_entry_t entry;
  int index;

  if (! menu)
    return 0;

  index = 0;
  entry = menu->entry_list;
  while (entry)
    {
      grub_uitree_t item;
      char *parm;
      char buf[8];

      item = grub_dialog_create ("template_menuitem", 1, 0, 0, 0);
      if (! item)
	return index;

      parm = grub_uitree_get_prop (item, "parameters");
      if (entry->title)
	grub_dialog_set_parm (item, parm, "title", entry->title);

      if (entry->classes->next)
	grub_dialog_set_parm (item, parm, "class", entry->classes->next->name);

      if (entry->users)
	grub_uitree_set_prop (item, "users", entry->users);

      grub_sprintf (buf, "%d", index);
      grub_uitree_set_prop (item, "index", buf);

      if (entry->save != -1)
	{
	  char v[2];

	  v[0] = '0' + entry->save;
	  v[1] = 0;
	  grub_uitree_set_prop (item, "save", v);
	}

      if (entry->sourcecode)
	grub_uitree_set_prop (item, "command", entry->sourcecode);

      grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (item), -1);
      if (index == default_num)
	grub_widget_select_node (item, 1);

      entry = entry->next;
      index++;
    }

  return index;
}

static grub_err_t
show_menu (grub_menu_t menu, int nested)
{
  grub_uitree_t root;

  root = get_screen ();
  if (! root)
    return grub_errno;

  if (! nested)
    {
      grub_uitree_t node;
      grub_term_output_t output;

      node = grub_uitree_find_id (root, "__menu__");
      if (node)
	{
	  int index, default_num;
	  char *default_str;

	  default_str = grub_env_get ("default");
	  default_num = (default_str) ? grub_strtol (default_str, 0, 0) : -1;
	  index = add_user_menu (node, menu, default_num);
	  add_sys_menu (node, index, default_num);
	}

      if (! grub_menu_region_get_current ())
	{
	  grub_command_execute ("menu_region.text", 0, 0);
	  if (grub_env_get ("gfxmode"))
	    grub_command_execute ("menu_region.gfx", 0, 0);
	}

      output = grub_term_get_current_output ();
      grub_command_execute ("terminal_output.gfxmenu", 0, 0);
      if (grub_widget_create (root) == GRUB_ERR_NONE)
	{
	  grub_widget_init (root);
	  grub_widget_input (root, 0);
	}
      grub_widget_free (root);
      grub_term_set_current_output (output);
    }
  else
    {
      grub_uitree_t node, child;

      node = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
      if (! node)
	return grub_errno;

      grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
      child = grub_uitree_find_id (node, "__child__");
      if (! child)
	child = node;
      add_user_menu (child, menu, -1);
      set_position (root, node);
      grub_dialog_popup (node);
      grub_dialog_free (node, 0, 0);
    }

  return 0;
}

struct grub_menu_viewer grub_ext_menu_viewer =
{
  .name = "ext",
  .show_menu = show_menu
};

static grub_extcmd_t cmd_popup, cmd_edit;
static grub_command_t cmd_refresh, cmd_toggle_mode;

GRUB_MOD_INIT(emenu)
{
  grub_menu_viewer_register ("ext", &grub_ext_menu_viewer);

  cmd_popup = grub_register_extcmd ("menu_popup", grub_cmd_popup,
				    GRUB_COMMAND_FLAG_BOTH,
				    "menu_popup [OPTIONS] NAME [PARM=VALUE]..",
				    "popup window", options);
  cmd_edit = grub_register_extcmd ("menu_edit", grub_cmd_popup,
				   GRUB_COMMAND_FLAG_BOTH,
				   "menu_edit [OPTIONS] NAME [PARM=PARM1]..",
				   "popup edit window", options);
  cmd_refresh = grub_register_command ("menu_refresh", grub_cmd_refresh,
				       0, "refresh menu");
  cmd_toggle_mode =
    grub_register_command ("menu_toggle_mode", grub_cmd_toggle_mode,
			   0, "toggle mode");
}

GRUB_MOD_FINI(emenu)
{
  grub_menu_viewer_unregister (&grub_ext_menu_viewer);

  grub_unregister_extcmd (cmd_popup);
  grub_unregister_extcmd (cmd_edit);
  grub_unregister_command (cmd_refresh);
  grub_unregister_command (cmd_toggle_mode);
}
