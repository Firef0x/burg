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
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/extcmd.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>

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
  char buf[12], *p;
  grub_widget_t screen, widget;
  int v;

  screen = root->data;
  widget = grub_widget_current_node->data;

  if (widget->org_x * 2 + widget->width <= screen->width)
    {
      v = widget->org_x + widget->width;
      p = "attach_left";
    }
  else
    {
      v = screen->width - widget->org_x;
      p = "attach_right";
    }
  grub_sprintf (buf, "%d/%d", v, v);
  grub_uitree_set_prop (node, p, buf);

  if (widget->org_y * 2 + widget->height <= screen->height)
    {
      v = widget->org_y + widget->height;
      p = "attach_top";
    }
  else
    {
      v = screen->height - widget->org_y;
      p = "attach_bottom";
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
grub_cmd_start (grub_command_t cmd __attribute__ ((unused)),
		int argc __attribute__ ((unused)),
		char **args __attribute__ ((unused)))
{
  grub_uint8_t c1, c2;
  grub_uitree_t node;
  grub_term_output_t output;

  node = get_screen ();
  if (! node)
    return grub_errno;

  grub_getcolor (&c1, &c2);
  output = grub_term_get_current_output ();
  grub_command_execute ("terminal_output.gfxmenu", 0, 0);
  if (grub_widget_create (node) == GRUB_ERR_NONE)
    {
      grub_widget_init (node);
      grub_widget_input (node, 0);
    }
  grub_widget_free (node);

  grub_term_set_current_output (output);
  grub_setcolor (c1, c2);
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  return grub_errno;
}

static grub_uitree_t
create_node (grub_uitree_t menu, grub_uitree_t tree, int *num,
	     const char *menu_template, const char *item_template)
{
  grub_uitree_t node;
  char *parm, *p;

  node = grub_dialog_create (item_template, 1, 0, 0, 0);
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

      sub = grub_dialog_create (menu_template, 1, 0, 0, 0);
      if (! sub)
	{
	  grub_uitree_free (node);
	  return 0;
	}

      grub_strcpy (buf, "menu_popup -");
      p = buf + sizeof ("menu_popup -") - 1;
      if (grub_uitree_get_prop (sub, "absolute") == 0)
	*(p++) = 'r';
      grub_sprintf (p, "i %d menu_tree", *num - menu->parent->flags - 1);
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

static grub_err_t
grub_cmd_create (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char **args)
{
  grub_uitree_t menu, node, tree;
  char *def;
  int num;

  if (argc < 2)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "not enough parameters");

  menu = grub_uitree_find (&grub_uitree_root, "menu");
  if (! menu)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "menu not found");

  node = get_screen ();
  if (! node)
    return grub_errno;

  node = grub_uitree_find_id (node, "__menu__");
  if (! node)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "id not found");

  tree = grub_uitree_find (&grub_uitree_root, "menu_tree");
  if (tree)
    {
      grub_tree_remove_node (GRUB_AS_TREE (tree));
      grub_uitree_free (tree);
    }
  tree = grub_uitree_create_node ("menu_tree");
  if (! tree)
    return grub_errno;
  grub_tree_add_child (GRUB_AS_TREE (&grub_uitree_root), GRUB_AS_TREE (tree),
		       -1);

  def = grub_env_get ("default");
  if (! def)
    def = "";
  
  num = 1;
  menu = menu->child;
  while (menu)
    {
      grub_uitree_t child;
      grub_uitree_t n;

      n = create_node (menu, tree, &num, args[0], args[1]);
      if (! n)
	return grub_errno;

      grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (n), -1);
      if (! grub_strcmp (menu->name, def))
	grub_widget_select_node (n, 1);
      
      child = menu->child;
      while (child)
	{
	  grub_uitree_t parent;

	  n = create_node (child, tree, &num, args[0], args[1]);
	  if (! n)
	    return grub_errno;

	  parent = child->parent->data;
	  if (! parent)
	    grub_error (GRUB_ERR_BAD_ARGUMENT, "parant shouldn't be null");

	  grub_tree_add_child (GRUB_AS_TREE (parent), GRUB_AS_TREE (n), -1);
	  child = grub_tree_next_node (GRUB_AS_TREE (menu),
				       GRUB_AS_TREE (child));
	}
      menu = menu->next;
    }

  return grub_errno;
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

static grub_extcmd_t cmd_popup, cmd_edit;
static grub_command_t cmd_start, cmd_create, cmd_refresh, cmd_toggle_mode;

GRUB_MOD_INIT(menucmd)
{
  cmd_popup = grub_register_extcmd ("menu_popup", grub_cmd_popup,
				    GRUB_COMMAND_FLAG_BOTH,
				    "menu_popup [OPTIONS] NAME [PARM=VALUE]..",
				    "popup window", options);
  cmd_edit = grub_register_extcmd ("menu_edit", grub_cmd_popup,
				   GRUB_COMMAND_FLAG_BOTH,
				   "menu_edit [OPTIONS] NAME [PARM=PARM1]..",
				   "popup edit window", options);

  cmd_start = grub_register_command ("menu_start", grub_cmd_start,
				     0, "start menu");
  cmd_create = grub_register_command ("menu_create", grub_cmd_create,
				      "menu_create MENU_TEMP ITEM_TEMP",
				      "create menu");
  cmd_refresh = grub_register_command ("menu_refresh", grub_cmd_refresh,
				       0, "refresh menu");
  cmd_toggle_mode =
    grub_register_command ("menu_toggle_mode", grub_cmd_toggle_mode,
			   0, "toggle mode");
}

GRUB_MOD_FINI(menucmd)
{
  grub_unregister_extcmd (cmd_popup);
  grub_unregister_extcmd (cmd_edit);
  grub_unregister_command (cmd_start);
  grub_unregister_command (cmd_create);
  grub_unregister_command (cmd_refresh);
  grub_unregister_command (cmd_toggle_mode);
}
