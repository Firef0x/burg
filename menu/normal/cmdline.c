/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2007  Free Software Foundation, Inc.
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

#include <grub/normal_menu.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/err.h>
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/partition.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/env.h>
#include <grub/lib.h>
#include <grub/i18n.h>

static char *kill_buf;

/* A completion hook to print items.  */
static void
print_completion (const char *item, grub_completion_type_t type, int count)
{
  if (count == 0)
    {
      /* If this is the first time, print a label.  */
      const char *what;

      switch (type)
	{
	case GRUB_COMPLETION_TYPE_COMMAND:
	  what = "commands";
	  break;
	case GRUB_COMPLETION_TYPE_DEVICE:
	  what = "devices";
	  break;
	case GRUB_COMPLETION_TYPE_FILE:
	  what = "files";
	  break;
	case GRUB_COMPLETION_TYPE_PARTITION:
	  what = "partitions";
	  break;
	case GRUB_COMPLETION_TYPE_ARGUMENT:
	  what = "arguments";
	  break;
	default:
	  what = "things";
	  break;
	}

      grub_printf ("\nPossible %s are:\n", what);
    }

  if (type == GRUB_COMPLETION_TYPE_PARTITION)
    {
      grub_print_device_info (item);
      grub_errno = GRUB_ERR_NONE;
    }
  else
    grub_printf (" %s", item);
}

struct grub_cmdline_get_closure
{
  unsigned max_len;
  int echo_char;
  char *buf;
  unsigned xpos, ypos, ystart;
  grub_size_t lpos, llen;
  grub_size_t plen;
};

static void
cl_set_pos (struct grub_cmdline_get_closure *c)
{
  c->xpos = (c->plen + c->lpos) % 79;
  c->ypos = c->ystart + (c->plen + c->lpos) / 79;
  grub_gotoxy (c->xpos, c->ypos);

  grub_refresh ();
}

static void
cl_print (int pos, int c, struct grub_cmdline_get_closure *cc)
{
  char *p;

  for (p = cc->buf + pos; *p; p++)
    {
      if ((cc->xpos)++ > 78)
	{
	  grub_putchar ('\n');

	  cc->xpos = 1;
	  if ((cc->ypos) == (unsigned) (grub_getxy () & 0xFF))
	    (cc->ystart)--;
	  else
	    (cc->ypos)++;
	}

      if (c)
	grub_putchar (c);
      else
	grub_putchar (*p);
    }
}

static void
cl_insert (const char *str, struct grub_cmdline_get_closure *c)
{
  grub_size_t len = grub_strlen (str);

  if (len + c->llen < c->max_len)
    {
      grub_memmove (c->buf + c->lpos + len, c->buf + c->lpos,
		    c->llen - c->lpos + 1);
      grub_memmove (c->buf + c->lpos, str, len);

      c->llen += len;
      c->lpos += len;
      cl_print (c->lpos - len, c->echo_char, c);
      cl_set_pos (c);
    }

  grub_refresh ();
}

static void
cl_delete (unsigned len, struct grub_cmdline_get_closure *c)
{
  if (c->lpos + len <= c->llen)
    {
      grub_size_t saved_lpos = c->lpos;

      c->lpos = c->llen - len;
      cl_set_pos (c);
      cl_print (c->lpos, ' ', c);
      c->lpos = saved_lpos;
      cl_set_pos (c);

      grub_memmove (c->buf + c->lpos, c->buf + c->lpos + len,
		    c->llen - c->lpos + 1);
      c->llen -= len;
      cl_print (c->lpos, c->echo_char, c);
      cl_set_pos (c);
    }

  grub_refresh ();
}

/* Get a command-line. If ECHO_CHAR is not zero, echo it instead of input
   characters. If READLINE is non-zero, readline-like key bindings are
   available. If ESC is pushed, return zero, otherwise return non-zero.  */
/* FIXME: The dumb interface is not supported yet.  */
int
grub_cmdline_get (const char *prompt, char cmdline[], unsigned max_len,
		  int echo_char, int readline, int history)
{
  char buf[max_len];
  int key;
  int histpos = 0;
  struct grub_cmdline_get_closure c;
  const char *prompt_translated = _(prompt);

  c.max_len = max_len;
  c.echo_char = echo_char;
  c.buf = buf;

  c.plen = grub_strlen (prompt_translated);
  c.lpos = c.llen = 0;
  buf[0] = '\0';

  if ((grub_getxy () >> 8) != 0)
    grub_putchar ('\n');

  grub_printf ("%s ", prompt_translated);

  c.xpos = c.plen;
  c.ystart = c.ypos = (grub_getxy () & 0xFF);

  cl_insert (cmdline, &c);

  if (history && grub_history_used () == 0)
    grub_history_add (buf);

  while ((key = GRUB_TERM_ASCII_CHAR (grub_getkey ())) != '\n' && key != '\r')
    {
      if (readline)
	{
	  switch (key)
	    {
	    case 1:	/* Ctrl-a */
	      c.lpos = 0;
	      cl_set_pos (&c);
	      break;

	    case 2:	/* Ctrl-b */
	      if (c.lpos > 0)
		{
		  c.lpos--;
		  cl_set_pos (&c);
		}
	      break;

	    case 5:	/* Ctrl-e */
	      c.lpos = c.llen;
	      cl_set_pos (&c);
	      break;

	    case 6:	/* Ctrl-f */
	      if (c.lpos < c.llen)
		{
		  c.lpos++;
		  cl_set_pos (&c);
		}
	      break;

	    case 9:	/* Ctrl-i or TAB */
	      {
		char *insert;
		int restore;

		/* Backup the next character and make it 0 so it will
		   be easy to use string functions.  */
		char backup = buf[c.lpos];
		buf[c.lpos] = '\0';

		insert = grub_complete (buf, &restore, print_completion);
		/* Restore the original string.  */
		buf[c.lpos] = backup;

		if (restore)
		  {
		    /* Restore the prompt.  */
		    grub_printf ("\n%s %s", prompt_translated, buf);
		    c.xpos = c.plen;
		    c.ystart = c.ypos = (grub_getxy () & 0xFF);
		  }

		if (insert)
		  {
		    cl_insert (insert, &c);
		    grub_free (insert);
		  }
	      }
	      break;

	    case 11:	/* Ctrl-k */
	      if (c.lpos < c.llen)
		{
		  if (kill_buf)
		    grub_free (kill_buf);

		  kill_buf = grub_strdup (buf + c.lpos);
		  grub_errno = GRUB_ERR_NONE;

		  cl_delete (c.llen - c.lpos, &c);
		}
	      break;

	    case GRUB_TERM_CTRL_N:	/* Ctrl-n */
	    case GRUB_TERM_DOWN:
	      {
		char *hist;

		c.lpos = 0;

		if (histpos > 0)
		  {
		    grub_history_replace (histpos, buf);
		    histpos--;
		  }

		cl_delete (c.llen, &c);
		hist = grub_history_get (histpos);
		cl_insert (hist, &c);

		break;
	      }
	    case GRUB_TERM_CTRL_P:	/* Ctrl-p */
	    case GRUB_TERM_UP:
	      {
		char *hist;

		c.lpos = 0;

		if (histpos < grub_history_used () - 1)
		  {
		    grub_history_replace (histpos, buf);
		    histpos++;
		  }

		cl_delete (c.llen, &c);
		hist = grub_history_get (histpos);

		cl_insert (hist, &c);
	      }
	      break;

	    case 21:	/* Ctrl-u */
	      if (c.lpos > 0)
		{
		  grub_size_t n = c.lpos;

		  if (kill_buf)
		    grub_free (kill_buf);

		  kill_buf = grub_malloc (n + 1);
		  grub_errno = GRUB_ERR_NONE;
		  if (kill_buf)
		    {
		      grub_memcpy (kill_buf, buf, n);
		      kill_buf[n] = '\0';
		    }

		  c.lpos = 0;
		  cl_set_pos (&c);
		  cl_delete (n, &c);
		}
	      break;

	    case 25:	/* Ctrl-y */
	      if (kill_buf)
		cl_insert (kill_buf, &c);
	      break;
	    }
	}

      switch (key)
	{
	case '\e':
	  return 0;

	case '\b':
	  if (c.lpos > 0)
	    {
	      c.lpos--;
	      cl_set_pos (&c);
	    }
          else
            break;
	  /* fall through */

	case 4:	/* Ctrl-d */
	  if (c.lpos < c.llen)
	    cl_delete (1, &c);
	  break;

	default:
	  if (grub_isprint (key))
	    {
	      char str[2];

	      str[0] = key;
	      str[1] = '\0';
	      cl_insert (str, &c);
	    }
	  break;
	}
    }

  grub_putchar ('\n');
  grub_refresh ();

  /* If ECHO_CHAR is NUL, remove leading spaces.  */
  c.lpos = 0;
  if (! echo_char)
    while (buf[c.lpos] == ' ')
      c.lpos++;

  if (history)
    {
      histpos = 0;
      if (grub_strlen (buf) > 0)
	{
	  grub_history_replace (histpos, buf);
	  grub_history_add ("");
	}
    }

  grub_memcpy (cmdline, buf + c.lpos, c.llen - c.lpos + 1);

  return 1;
}
