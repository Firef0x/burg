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
#include <grub/lib.h>
#include <grub/misc.h>

GRUB_EXPORT(grub_history_init);
GRUB_EXPORT(grub_history_get);
GRUB_EXPORT(grub_history_add);
GRUB_EXPORT(grub_history_replace);
GRUB_EXPORT(grub_history_used);

static int hist_size;
static char **hist_lines = 0;
static int hist_pos = 0;
static int hist_end = 0;
static int hist_used = 0;

grub_err_t
grub_history_init (int newsize)
{
  char **old_hist_lines = hist_lines;
  hist_lines = grub_malloc (sizeof (char *) * newsize);

  /* Copy the old lines into the new buffer.  */
  if (old_hist_lines)
    {
      /* Remove the lines that don't fit in the new buffer.  */
      if (newsize < hist_used)
	{
	  int i;
	  int delsize = hist_used - newsize;
	  hist_used = newsize;

	  for (i = 1; i <= delsize; i++)
	    {
	      int pos = hist_end - i;
	      if (pos < 0)
		pos += hist_size;
	      grub_free (old_hist_lines[pos]);
	    }

	  hist_end -= delsize;
	  if (hist_end < 0)
	    hist_end += hist_size;
	}

      if (hist_pos < hist_end)
	grub_memmove (hist_lines, old_hist_lines + hist_pos,
		      (hist_end - hist_pos) * sizeof (char *));
      else if (hist_used)
	{
	  /* Copy the older part.  */
	  grub_memmove (hist_lines, old_hist_lines + hist_pos,
 			(hist_size - hist_pos) * sizeof (char *));

	  /* Copy the newer part. */
	  grub_memmove (hist_lines + hist_size - hist_pos, old_hist_lines,
			hist_end * sizeof (char *));
	}
    }

  grub_free (old_hist_lines);

  hist_size = newsize;
  hist_pos = 0;
  hist_end = hist_used;
  return 0;
}

/* Get the entry POS from the history where `0' is the newest
   entry.  */
char *
grub_history_get (int pos)
{
  pos = (hist_pos + pos) % hist_size;
  return hist_lines[pos];
}

/* Insert a new history line S on the top of the history.  */
void
grub_history_add (char *s)
{
  /* Remove the oldest entry in the history to make room for a new
     entry.  */
  if (hist_used + 1 > hist_size)
    {
      hist_end--;
      if (hist_end < 0)
	hist_end = hist_size + hist_end;

      grub_free (hist_lines[hist_end]);
    }
  else
    hist_used++;

  /* Move to the next position.  */
  hist_pos--;
  if (hist_pos < 0)
    hist_pos = hist_size + hist_pos;

  /* Insert into history.  */
  hist_lines[hist_pos] = grub_strdup (s);
}

/* Replace the history entry on position POS with the string S.  */
void
grub_history_replace (int pos, char *s)
{
  pos = (hist_pos + pos) % hist_size;
  grub_free (hist_lines[pos]);
  hist_lines[pos] = grub_strdup (s);
}

int
grub_history_used (void)
{
  return hist_used;
}
