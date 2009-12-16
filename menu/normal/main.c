/* menu_text.c - Basic text menu implementation.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/lib.h>
#include <grub/auth.h>
#include <grub/reader.h>
#include <grub/parser.h>
#include <grub/normal_menu.h>
#include <grub/menu_viewer.h>
#include <grub/i18n.h>

/* Time to delay after displaying an error message about a default/fallback
   entry failing to boot.  */
#define DEFAULT_ENTRY_ERROR_DELAY_MS  2500

static grub_uint8_t grub_color_menu_normal;
static grub_uint8_t grub_color_menu_highlight;

static void
print_spaces (int number_spaces)
{
  int i;
  for (i = 0; i < number_spaces; i++)
    grub_putchar (' ');
}

static void
grub_print_ucs4 (const grub_uint32_t * str,
                const grub_uint32_t * last_position)
{
  while (str < last_position)
    {
      grub_putcode (*str);
      str++;
    }
}

static grub_ssize_t
getstringwidth (grub_uint32_t * str, const grub_uint32_t * last_position)
{
  grub_ssize_t width = 0;

  while (str < last_position)
    {
      width += grub_getcharwidth (*str);
      str++;
    }
  return width;
}

static void
print_message_indented (const char *msg)
{
  const int line_len = GRUB_TERM_WIDTH - grub_getcharwidth ('m') * 15;

  grub_uint32_t *unicode_msg;

  grub_ssize_t msg_len = grub_strlen (msg);

  unicode_msg = grub_malloc (msg_len * sizeof (*unicode_msg));

  msg_len = grub_utf8_to_ucs4 (unicode_msg, msg_len,
                              (grub_uint8_t *) msg, -1, 0);

  if (!unicode_msg)
    {
      grub_printf ("print_message_indented ERROR1: %s", msg);
      return;
    }

  if (msg_len < 0)
    {
      grub_printf ("print_message_indented ERROR2: %s", msg);
      grub_free (unicode_msg);
      return;
    }

  const grub_uint32_t *last_position = unicode_msg + msg_len;

  grub_uint32_t *current_position = unicode_msg;

  grub_uint32_t *next_new_line = unicode_msg;

  while (current_position < last_position)
    {
      next_new_line = (grub_uint32_t *) last_position;

      while (getstringwidth (current_position, next_new_line) > line_len
            || (*next_new_line != ' ' && next_new_line > current_position &&
                next_new_line != last_position))
       {
         next_new_line--;
       }

      if (next_new_line == current_position)
       {
         next_new_line = (next_new_line + line_len > last_position) ?
           (grub_uint32_t *) last_position : next_new_line + line_len;
       }

      print_spaces (6);
      grub_print_ucs4 (current_position, next_new_line);
      grub_putchar ('\n');

      next_new_line++;
      current_position = next_new_line;
    }
  grub_free (unicode_msg);
}

static void
draw_border (void)
{
  unsigned i;

  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  grub_gotoxy (GRUB_TERM_MARGIN, GRUB_TERM_TOP_BORDER_Y);
  grub_putcode (GRUB_TERM_DISP_UL);
  for (i = 0; i < (unsigned) GRUB_TERM_BORDER_WIDTH - 2; i++)
    grub_putcode (GRUB_TERM_DISP_HLINE);
  grub_putcode (GRUB_TERM_DISP_UR);

  for (i = 0; i < (unsigned) GRUB_TERM_NUM_ENTRIES; i++)
    {
      grub_gotoxy (GRUB_TERM_MARGIN, GRUB_TERM_TOP_BORDER_Y + i + 1);
      grub_putcode (GRUB_TERM_DISP_VLINE);
      grub_gotoxy (GRUB_TERM_MARGIN + GRUB_TERM_BORDER_WIDTH - 1,
		   GRUB_TERM_TOP_BORDER_Y + i + 1);
      grub_putcode (GRUB_TERM_DISP_VLINE);
    }

  grub_gotoxy (GRUB_TERM_MARGIN,
	       GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES + 1);
  grub_putcode (GRUB_TERM_DISP_LL);
  for (i = 0; i < (unsigned) GRUB_TERM_BORDER_WIDTH - 2; i++)
    grub_putcode (GRUB_TERM_DISP_HLINE);
  grub_putcode (GRUB_TERM_DISP_LR);

  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  grub_gotoxy (GRUB_TERM_MARGIN,
	       (GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES
		+ GRUB_TERM_MARGIN + 1));
}

static void
print_message (int nested, int edit)
{
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  if (edit)
    {
      grub_putchar ('\n');
      print_message_indented (_("Minimum Emacs-like screen editing is \
supported. TAB lists completions. Press Ctrl-x to boot, Ctrl-c for a \
command-line or ESC to return menu."));
    }
  else
    {
      const char *msg = _("Use the %C and %C keys to select which \
entry is highlighted.");
      char *msg_translated =
       grub_malloc (sizeof (char) * grub_strlen (msg) + 1);

      grub_sprintf (msg_translated, msg, (grub_uint32_t) GRUB_TERM_DISP_UP,
                   (grub_uint32_t) GRUB_TERM_DISP_DOWN);
      grub_putchar ('\n');
      print_message_indented (msg_translated);

      grub_free (msg_translated);

      print_message_indented (_("Press enter to boot the selected OS, \
\'e\' to edit the commands before booting or \'c\' for a command-line."));

      if (nested)
        {
          grub_printf ("\n        ");
          grub_printf_ (N_("ESC to return previous menu."));
        }
    }
}

static void
print_entry (int y, int highlight, grub_menu_entry_t entry)
{
  int x;
  const char *title;
  grub_size_t title_len;
  grub_ssize_t len;
  grub_uint32_t *unicode_title;
  grub_ssize_t i;
  grub_uint8_t old_color_normal, old_color_highlight;

  title = entry ? entry->title : "";
  title_len = grub_strlen (title);
  unicode_title = grub_malloc (title_len * sizeof (*unicode_title));
  if (! unicode_title)
    /* XXX How to show this error?  */
    return;

  len = grub_utf8_to_ucs4 (unicode_title, title_len,
			   (grub_uint8_t *) title, -1, 0);
  if (len < 0)
    {
      /* It is an invalid sequence.  */
      grub_free (unicode_title);
      return;
    }

  grub_getcolor (&old_color_normal, &old_color_highlight);
  grub_setcolor (grub_color_menu_normal, grub_color_menu_highlight);
  grub_setcolorstate (highlight
		      ? GRUB_TERM_COLOR_HIGHLIGHT
		      : GRUB_TERM_COLOR_NORMAL);

  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_MARGIN, y);

  for (x = GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_MARGIN + 1, i = 0;
       x < GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH - GRUB_TERM_MARGIN;
       i++)
    {
      if (i < len
	  && x <= (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH
		   - GRUB_TERM_MARGIN - 1))
	{
	  grub_ssize_t width;

	  width = grub_getcharwidth (unicode_title[i]);

	  if (x + width > (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH
			   - GRUB_TERM_MARGIN - 1))
	    grub_putcode (GRUB_TERM_DISP_RIGHT);
	  else
	    grub_putcode (unicode_title[i]);

	  x += width;
	}
      else
	{
	  grub_putchar (' ');
	  x++;
	}
    }
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);
  grub_putchar (' ');

  grub_gotoxy (GRUB_TERM_CURSOR_X, y);

  grub_setcolor (old_color_normal, old_color_highlight);
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);
  grub_free (unicode_title);
}

static void
print_entries (grub_menu_t menu, int first, int offset)
{
  grub_menu_entry_t e;
  int i;

  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH,
	       GRUB_TERM_FIRST_ENTRY_Y);

  if (first)
    grub_putcode (GRUB_TERM_DISP_UP);
  else
    grub_putchar (' ');

  e = grub_menu_get_entry (menu, first);

  for (i = 0; i < GRUB_TERM_NUM_ENTRIES; i++)
    {
      print_entry (GRUB_TERM_FIRST_ENTRY_Y + i, offset == i, e);
      if (e)
	e = e->next;
    }

  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH,
	       GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES);

  if (e)
    grub_putcode (GRUB_TERM_DISP_DOWN);
  else
    grub_putchar (' ');

  grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
}

/* Initialize the screen.  If NESTED is non-zero, assume that this menu
   is run from another menu or a command-line. If EDIT is non-zero, show
   a message for the menu entry editor.  */
void
grub_menu_init_page (int nested, int edit)
{
  grub_uint8_t old_color_normal, old_color_highlight;

  grub_getcolor (&old_color_normal, &old_color_highlight);

  /* By default, use the same colors for the menu.  */
  grub_color_menu_normal = old_color_normal;
  grub_color_menu_highlight = old_color_highlight;

  /* Then give user a chance to replace them.  */
  grub_parse_color_name_pair (&grub_color_menu_normal, grub_env_get ("menu_color_normal"));
  grub_parse_color_name_pair (&grub_color_menu_highlight, grub_env_get ("menu_color_highlight"));

  grub_normal_init_page ();
  grub_setcolor (grub_color_menu_normal, grub_color_menu_highlight);
  draw_border ();
  grub_setcolor (old_color_normal, old_color_highlight);
  print_message (nested, edit);
}

/* Get the entry number from the variable NAME.  */
static int
get_entry_number (const char *name)
{
  char *val;
  int entry;

  val = grub_env_get (name);
  if (! val)
    return -1;

  grub_error_push ();

  entry = (int) grub_strtoul (val, 0, 0);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_errno = GRUB_ERR_NONE;
      entry = -1;
    }

  grub_error_pop ();

  return entry;
}

static void
print_timeout (int timeout, int offset, int second_stage)
{
  const char *msg =
    _("The highlighted entry will be booted automatically in %ds.");
  const int msg_localized_len = grub_strlen (msg);
  const int number_spaces = GRUB_TERM_WIDTH - msg_localized_len - 3;

  char *msg_end = grub_strchr (msg, '%');

  grub_gotoxy (second_stage ? (msg_end - msg + 3) : 3, GRUB_TERM_HEIGHT - 3);
  grub_printf (second_stage ? msg_end : msg, timeout);
  print_spaces (second_stage ? number_spaces : 0);

  grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
  grub_refresh ();
};

/* Show the menu and handle menu entry selection.  Returns the menu entry
   index that should be executed or -1 if no entry should be executed (e.g.,
   Esc pressed to exit a sub-menu or switching menu viewers).
   If the return value is not -1, then *AUTO_BOOT is nonzero iff the menu
   entry to be executed is a result of an automatic default selection because
   of the timeout.  */
static int
run_menu (grub_menu_t menu, int nested, int *auto_boot)
{
  int first, offset;
  grub_uint64_t saved_time;
  int default_entry;
  int timeout;

  first = 0;

  default_entry = get_entry_number ("default");

  /* If DEFAULT_ENTRY is not within the menu entries, fall back to
     the first entry.  */
  if (default_entry < 0 || default_entry >= menu->size)
    default_entry = 0;

  /* If timeout is 0, drawing is pointless (and ugly).  */
  if (grub_menu_get_timeout () == 0)
    {
      *auto_boot = 1;
      return default_entry;
    }

  offset = default_entry;
  if (offset > GRUB_TERM_NUM_ENTRIES - 1)
    {
      first = offset - (GRUB_TERM_NUM_ENTRIES - 1);
      offset = GRUB_TERM_NUM_ENTRIES - 1;
    }

  /* Initialize the time.  */
  saved_time = grub_get_time_ms ();

 refresh:
  grub_setcursor (0);
  grub_menu_init_page (nested, 0);
  print_entries (menu, first, offset);
  grub_refresh ();

  timeout = grub_menu_get_timeout ();

  if (timeout > 0)
    print_timeout (timeout, offset, 0);

  while (1)
    {
      int c;
      timeout = grub_menu_get_timeout ();

      if (timeout > 0)
	{
	  grub_uint64_t current_time;

	  current_time = grub_get_time_ms ();
	  if (current_time - saved_time >= 1000)
	    {
	      timeout--;
	      grub_menu_set_timeout (timeout);
	      saved_time = current_time;
	      print_timeout (timeout, offset, 1);
	    }
	}

      if (timeout == 0)
	{
	  grub_env_unset ("timeout");
	  *auto_boot = 1;
	  return default_entry;
	}

      if (grub_checkkey () >= 0 || timeout < 0)
	{
	  c = GRUB_TERM_ASCII_CHAR (grub_getkey ());

	  if (timeout >= 0)
	    {
	      grub_gotoxy (0, GRUB_TERM_HEIGHT - 3);
              print_spaces (GRUB_TERM_WIDTH - 1);

	      grub_env_unset ("timeout");
	      grub_env_unset ("fallback");
	      grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
	    }

	  switch (c)
	    {
	    case GRUB_TERM_HOME:
	      first = 0;
	      offset = 0;
	      print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_END:
	      offset = menu->size - 1;
	      if (offset > GRUB_TERM_NUM_ENTRIES - 1)
		{
		  first = offset - (GRUB_TERM_NUM_ENTRIES - 1);
		  offset = GRUB_TERM_NUM_ENTRIES - 1;
		}
		print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_UP:
	    case '^':
	      if (offset > 0)
		{
		  print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 0,
			       grub_menu_get_entry (menu, first + offset));
		  offset--;
		  print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 1,
			       grub_menu_get_entry (menu, first + offset));
		}
	      else if (first > 0)
		{
		  first--;
		  print_entries (menu, first, offset);
		}
	      break;

	    case GRUB_TERM_DOWN:
	    case 'v':
	      if (menu->size > first + offset + 1)
		{
		  if (offset < GRUB_TERM_NUM_ENTRIES - 1)
		    {
		      print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 0,
				   grub_menu_get_entry (menu, first + offset));
		      offset++;
		      print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 1,
				   grub_menu_get_entry (menu, first + offset));
		    }
		  else
		    {
		      first++;
		      print_entries (menu, first, offset);
		    }
		}
	      break;

	    case GRUB_TERM_PPAGE:
	      if (first == 0)
		{
		  offset = 0;
		}
	      else
		{
		  first -= GRUB_TERM_NUM_ENTRIES;

		  if (first < 0)
		    {
		      offset += first;
		      first = 0;
		    }
		}
	      print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_NPAGE:
	      if (offset == 0)
		{
		  offset += GRUB_TERM_NUM_ENTRIES - 1;
		  if (first + offset >= menu->size)
		    {
		      offset = menu->size - first - 1;
		    }
		}
	      else
		{
		  first += GRUB_TERM_NUM_ENTRIES;

		  if (first + offset >= menu->size)
		    {
		      first -= GRUB_TERM_NUM_ENTRIES;
		      offset += GRUB_TERM_NUM_ENTRIES;

		      if (offset > menu->size - 1 ||
			  offset > GRUB_TERM_NUM_ENTRIES - 1)
			{
			  offset = menu->size - first - 1;
			}
		      if (offset > GRUB_TERM_NUM_ENTRIES)
			{
			  first += offset - GRUB_TERM_NUM_ENTRIES + 1;
			  offset = GRUB_TERM_NUM_ENTRIES - 1;
			}
		    }
		}
	      print_entries (menu, first, offset);
	      break;

	    case '\n':
	    case '\r':
	    case 6:
	      grub_setcursor (1);
	      *auto_boot = 0;
	      return first + offset;

	    case '\e':
	      if (nested)
		{
		  grub_setcursor (1);
		  return -1;
		}
	      break;

	    case 'c':
	      grub_cmdline_run (1);
	      goto refresh;

	    case 'e':
		{
		  grub_menu_entry_t e = grub_menu_get_entry (menu, first + offset);
		  if (e)
		    grub_menu_entry_run (e);
		}
	      goto refresh;

	    default:
	      break;
	    }

	  grub_refresh ();
	}
    }

  /* Never reach here.  */
  return -1;
}

/* Callback invoked immediately before a menu entry is executed.  */
static void
notify_booting (grub_menu_entry_t entry,
		void *userdata __attribute__((unused)))
{
  grub_printf ("  ");
  grub_printf_ (N_("Booting \'%s\'"), entry->title);
  grub_printf ("\n\n");
}

/* Callback invoked when a default menu entry executed because of a timeout
   has failed and an attempt will be made to execute the next fallback
   entry, ENTRY.  */
static void
notify_fallback (grub_menu_entry_t entry,
		 void *userdata __attribute__((unused)))
{
  grub_printf ("\n   ");
  grub_printf_ (N_("Falling back to \'%s\'"), entry->title);
  grub_printf ("\n\n");
  grub_millisleep (DEFAULT_ENTRY_ERROR_DELAY_MS);
}

/* Callback invoked when a menu entry has failed and there is no remaining
   fallback entry to attempt.  */
static void
notify_execution_failure (void *userdata __attribute__((unused)))
{
  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
    }
  grub_printf ("\n  ");
  grub_printf_ (N_("Failed to boot default entries.\n"));
  grub_wait_after_message ();
}

/* Callbacks used by the text menu to provide user feedback when menu entries
   are executed.  */
static struct grub_menu_execute_callback execution_callback =
{
  .notify_booting = notify_booting,
  .notify_fallback = notify_fallback,
  .notify_failure = notify_execution_failure
};

static grub_err_t
show_text_menu (grub_menu_t menu, int nested)
{
  if ((! menu) && (! nested))
    {
      grub_cmdline_run (0);
      return 0;
    }

  while (1)
    {
      int boot_entry;
      grub_menu_entry_t e;
      int auto_boot;

      boot_entry = run_menu (menu, nested, &auto_boot);
      if (boot_entry < 0)
	break;

      e = grub_menu_get_entry (menu, boot_entry);
      if (! e)
	continue; /* Menu is empty.  */

      grub_cls ();
      grub_setcursor (1);

      if (auto_boot)
	{
	  grub_menu_execute_with_fallback (menu, e, &execution_callback, 0);
	}
      else
	{
	  grub_errno = GRUB_ERR_NONE;
	  grub_menu_execute_entry (e);
	  if (grub_errno != GRUB_ERR_NONE)
	    {
	      grub_print_error ();
	      grub_errno = GRUB_ERR_NONE;
	      grub_wait_after_message ();
	    }
	}
    }

  return GRUB_ERR_NONE;
}

struct grub_menu_viewer grub_normal_text_menu_viewer =
{
  .name = "normal",
  .show_menu = show_text_menu
};

/* Initialize the screen.  */
void
grub_normal_init_page (void)
{
  grub_uint8_t width, margin;

#define TITLE ("GNU GRUB  version " PACKAGE_VERSION)

  width = grub_getwh () >> 8;
  margin = (width - (sizeof(TITLE) + 7)) / 2;

  grub_cls ();
  grub_putchar ('\n');

  while (margin--)
    grub_putchar (' ');

  grub_printf ("%s\n\n", TITLE);

#undef TITLE
}

grub_err_t
grub_normal_check_authentication (const char *userlist)
{
  char login[1024], entered[1024];

  if (grub_auth_check_password (userlist, 0, 0))
    return GRUB_ERR_NONE;

  grub_memset (login, 0, sizeof (login));
  if (!grub_cmdline_get ("Enter username: ", login, sizeof (login) - 1,
			 0, 0, 0))
    return GRUB_ACCESS_DENIED;

  grub_memset (&entered, 0, sizeof (entered));
  if (!grub_cmdline_get ("Enter password: ",  entered, sizeof (entered) - 1,
			 '*', 0, 0))
    return GRUB_ACCESS_DENIED;

  return (grub_auth_check_password (userlist, login, entered)) ?
    GRUB_ERR_NONE : GRUB_ACCESS_DENIED;
}

static char cmdline[GRUB_MAX_CMDLINE];

int reader_nested;

static grub_err_t
grub_normal_read_line (char **line, int cont)
{
  grub_parser_t parser = grub_parser_get_current ();
  char prompt[sizeof("> ") + grub_strlen (parser->name)];

  grub_sprintf (prompt, "%s> ", parser->name);

  while (1)
    {
      cmdline[0] = 0;
      if (grub_cmdline_get (prompt, cmdline, sizeof (cmdline), 0, 1, 1))
	break;

      if ((reader_nested) || (cont))
	{
	  *line = 0;
	  return grub_errno;
	}
    }

  *line = grub_strdup (cmdline);
  return 0;
}

void
grub_cmdline_run (int nested)
{
  grub_err_t err = GRUB_ERR_NONE;

  err = grub_normal_check_authentication (NULL);

  if (err)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  grub_normal_init_page ();
  grub_setcursor (1);

  grub_printf_ (N_("\
 [ Minimal BASH-like line editing is supported. For the first word, TAB\n\
   lists possible command completions. Anywhere else TAB lists possible\n\
   device/file completions.%s ]\n\n"),
	       nested ? " ESC at any time exits." : "");

  reader_nested = nested;
  grub_reader_loop (grub_normal_read_line);
}

static char *
grub_env_write_pager (struct grub_env_var *var __attribute__ ((unused)),
		      const char *val)
{
  grub_set_more ((*val == '1'));
  return grub_strdup (val);
}

GRUB_MOD_INIT(nmenu)
{
  grub_menu_viewer_register ("normal", &grub_normal_text_menu_viewer);

  grub_register_variable_hook ("pager", 0, grub_env_write_pager);

  /* Reload terminal colors when these variables are written to.  */
  grub_register_variable_hook ("color_normal", NULL,
			       grub_env_write_color_normal);
  grub_register_variable_hook ("color_highlight", NULL,
			       grub_env_write_color_highlight);

  /* Preserve hooks after context changes.  */
  grub_env_export ("color_normal");
  grub_env_export ("color_highlight");
}

GRUB_MOD_FINI(nmenu)
{
  grub_menu_viewer_unregister (&grub_normal_text_menu_viewer);

  grub_register_variable_hook ("pager", 0, 0);
  grub_register_variable_hook ("color_normal", 0, 0);
  grub_register_variable_hook ("color_highlight", 0, 0);
}
