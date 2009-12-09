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
#include <grub/env.h>
#include <grub/lib.h>
#include <grub/menu.h>
#include <grub/misc.h>
#include <grub/parser.h>
#include <grub/command.h>
#include <grub/menu_viewer.h>
#include <grub/uitree.h>

GRUB_EXPORT(grub_menu_entry_add);
GRUB_EXPORT(grub_menu_execute);

static void
free_menu (grub_menu_t menu)
{
  grub_menu_entry_t entry;

  if (! menu)
    return;

  entry = menu->entry_list;
  while (entry)
    {
      grub_menu_entry_t next_entry = entry->next;

      grub_free ((void *) entry->title);
      grub_free ((void *) entry->sourcecode);
      entry = next_entry;
    }

  grub_free (menu);
  grub_env_unset_data_slot ("menu");
}

static void
free_menu_entry_classes (struct grub_menu_entry_class *head)
{
  /* Free all the classes.  */
  while (head)
    {
      struct grub_menu_entry_class *next;

      grub_free (head->name);
      next = head->next;
      grub_free (head);
      head = next;
    }
}

/* Add a menu entry to the current menu context (as given by the environment
   variable data slot `menu').  As the configuration file is read, the script
   parser calls this when a menu entry is to be created.  */
grub_err_t
grub_menu_entry_add (int argc, const char **args, const char *sourcecode)
{
  const char *menutitle = 0;
  const char *menusourcecode;
  grub_menu_t menu;
  grub_menu_entry_t *last;
  int failed = 0;
  int i;
  struct grub_menu_entry_class *classes_head;  /* Dummy head node for list.  */
  struct grub_menu_entry_class *classes_tail;
  char *users = NULL;
  int save = -1;

  /* Allocate dummy head node for class list.  */
  classes_head = grub_zalloc (sizeof (struct grub_menu_entry_class));
  if (! classes_head)
    return grub_errno;
  classes_tail = classes_head;

  menu = grub_env_get_data_slot ("menu");
  if (! menu)
    return grub_error (GRUB_ERR_MENU, "no menu context");

  last = &menu->entry_list;

  menusourcecode = grub_strdup (sourcecode);
  if (! menusourcecode)
    return grub_errno;

  /* Parse menu arguments.  */
  for (i = 0; i < argc; i++)
    {
      /* Capture arguments.  */
      if (grub_strncmp ("--", args[i], 2) == 0)
	{
	  const char *arg = &args[i][2];

	  /* Handle menu class.  */
	  if (grub_strcmp(arg, "class") == 0)
	    {
	      char *class_name;
	      struct grub_menu_entry_class *new_class;

	      i++;
	      class_name = grub_strdup (args[i]);
	      if (! class_name)
		{
		  failed = 1;
		  break;
		}

	      /* Create a new class and add it at the tail of the list.  */
	      new_class = grub_zalloc (sizeof (struct grub_menu_entry_class));
	      if (! new_class)
		{
		  grub_free (class_name);
		  failed = 1;
		  break;
		}
	      /* Fill in the new class node.  */
	      new_class->name = class_name;
	      /* Link the tail to it, and make it the new tail.  */
	      classes_tail->next = new_class;
	      classes_tail = new_class;
	      continue;
	    }
	  else if (grub_strcmp(arg, "users") == 0)
	    {
	      i++;
	      users = grub_strdup (args[i]);
	      if (! users)
		{
		  failed = 1;
		  break;
		}

	      continue;
	    }
	  else if (grub_strcmp(arg, "save") == 0)
	    {
	      save = 1;
	      continue;
	    }
	  else if (grub_strcmp(arg, "nosave") == 0)
	    {
	      save = 0;
	      continue;
	    }
	  else
	    {
	      /* Handle invalid argument.  */
	      failed = 1;
	      grub_error (GRUB_ERR_MENU,
			  "invalid argument for menuentry: %s", args[i]);
	      break;
	    }
	}

      /* Capture title.  */
      if (! menutitle)
	{
	  menutitle = grub_strdup (args[i]);
	}
      else
	{
	  failed = 1;
	  grub_error (GRUB_ERR_MENU,
		      "too many titles for menuentry: %s", args[i]);
	  break;
	}
    }

  /* Validate arguments.  */
  if ((! failed) && (! menutitle))
    {
      grub_error (GRUB_ERR_MENU, "menuentry is missing title");
      failed = 1;
    }

  /* If argument parsing failed, free any allocated resources.  */
  if (failed)
    {
      free_menu_entry_classes (classes_head);
      grub_free ((void *) menutitle);
      grub_free ((void *) menusourcecode);

      /* Here we assume that grub_error has been used to specify failure details.  */
      return grub_errno;
    }

  /* Add the menu entry at the end of the list.  */
  while (*last)
    last = &(*last)->next;

  *last = grub_zalloc (sizeof (**last));
  if (! *last)
    {
      free_menu_entry_classes (classes_head);
      grub_free ((void *) menutitle);
      grub_free ((void *) menusourcecode);
      return grub_errno;
    }

  (*last)->title = menutitle;
  (*last)->classes = classes_head;
  if (users)
    (*last)->restricted = 1;
  (*last)->users = users;
  (*last)->sourcecode = menusourcecode;
  (*last)->save = save;

  menu->size++;

  return grub_errno;
}

static grub_menu_t
read_config_file (const char *config)
{
  grub_file_t file;
  grub_parser_t old_parser = 0;

  auto grub_err_t getline (char **line, int cont);
  grub_err_t getline (char **line, int cont __attribute__ ((unused)))
    {
      while (1)
	{
	  char *buf;

	  *line = buf = grub_getline (file);
	  if (! buf)
	    return grub_errno;

	  if (buf[0] == '#')
	    {
	      if (buf[1] == '!')
		{
		  grub_parser_t parser;
		  grub_named_list_t list;

		  buf += 2;
		  while (grub_isspace (*buf))
		    buf++;

		  if (! old_parser)
		    old_parser = grub_parser_get_current ();

		  list = GRUB_AS_NAMED_LIST (grub_parser_class.handler_list);
		  parser = grub_named_list_find (list, buf);
		  if (parser)
		    grub_parser_set_current (parser);
		  else
		    {
		      char cmd_name[8 + grub_strlen (buf)];

		      /* Perhaps it's not loaded yet, try the autoload
			 command.  */
		      grub_strcpy (cmd_name, "parser.");
		      grub_strcat (cmd_name, buf);
		      grub_command_execute (cmd_name, 0, 0);
		    }
		}
	      grub_free (*line);
	    }
	  else
	    break;
	}

      return GRUB_ERR_NONE;
    }

  grub_menu_t newmenu;

  newmenu = grub_env_get_data_slot ("menu");
  if (! newmenu)
    {
      newmenu = grub_zalloc (sizeof (*newmenu));
      if (! newmenu)
	return 0;

      grub_env_set_data_slot ("menu", newmenu);
    }

  /* Try to open the config file.  */
  file = grub_file_open (config);
  if (! file)
    return 0;

  grub_reader_loop (getline);
  grub_file_close (file);

  if (old_parser)
    grub_parser_set_current (old_parser);

  return newmenu;
}

/* Read the config file CONFIG and execute the menu interface or
   the command line interface if BATCH is false.  */
void
grub_menu_execute (const char *config, int nested, int batch)
{
  grub_menu_t menu = 0;

  if (config)
    {
      menu = read_config_file (config);

      /* Ignore any error.  */
      grub_errno = GRUB_ERR_NONE;
    }

  if (! batch)
    {
      if (menu && menu->size)
	{
	  if (grub_menu_viewer_get_current ())
	    grub_menu_viewer_get_current ()->show_menu (menu, nested);

	  free_menu (menu);
	}
      else if (! nested)
	{
	  if (grub_menu_viewer_get_current ())
	    grub_menu_viewer_get_current ()->show_menu (0, 0);
	}
    }
}
