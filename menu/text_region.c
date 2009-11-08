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
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/menu_region.h>

static struct grub_menu_region grub_text_region;
static grub_term_output_t term;

static grub_err_t
grub_text_region_init (void)
{
  grub_text_region.char_width = grub_text_region.char_height = 1;
  return 0;
}

static void
grub_text_region_get_screen_size (int *width, int *height)
{
  int wh = term->getwh ();

  *width = ((wh >> 8) & 0xff) - 1;
  *height = wh & 0xff;
}

static int
grub_text_region_get_text_width (grub_font_t font __attribute__ ((unused)),
				 const char *str, int count, int *chars)
{
  int len;

  len = (count) ? count : (int) grub_strlen (str);
  if (chars)
    *chars = len;

  return len;
}

static int
grub_text_region_get_text_height (grub_font_t font __attribute__ ((unused)))
{
  return 1;
}

static grub_video_color_t
grub_text_region_map_color (int fg_color, int bg_color)
{
  return ((bg_color & 7) << 4) + fg_color;
}

static void
grub_text_region_update_rect (struct grub_menu_region_rect *rect,
			      int x __attribute__ ((unused)),
			      int y __attribute__ ((unused)),
			      int width, int height, int scn_x, int scn_y)
{
  int w, h;
  grub_uint32_t fill;

  fill = (rect->fill) ? rect->fill : ' ';
  term->setcolor (rect->color, 0);
  term->setcolorstate (GRUB_TERM_COLOR_NORMAL);
  for (h = 0; h < height; h++)
    {
      term->gotoxy (scn_x, scn_y);

      for (w = 0; w < width; w++)
	term->putchar (fill);

      scn_y++;
    }
}

static void
grub_text_region_update_text (struct grub_menu_region_text *text,
			      int x, int y __attribute__ ((unused)),
			      int width, int height __attribute__ ((unused)),
			      int scn_x, int scn_y)
{
  char *s;
  int i;

  term->setcolor (text->color, 0);
  term->setcolorstate (GRUB_TERM_COLOR_NORMAL);
  term->gotoxy (scn_x, scn_y);
  s = text->text + x;
  for (i = 0; i < width; i++, s++)
    term->putchar (*s);
}

static void
grub_text_region_hide_cursor (void)
{
  term->setcursor (0);
}

static void
grub_text_region_draw_cursor (struct grub_menu_region_text *text
			      __attribute__ ((unused)),
			      int width __attribute__ ((unused)),
			      int height __attribute__ ((unused)),
			      int scn_x, int scn_y)
{
  term->gotoxy (scn_x, scn_y);
  term->setcursor (1);
}

static struct grub_menu_region grub_text_region =
  {
    .name = "text",
    .init = grub_text_region_init,
    .get_screen_size = grub_text_region_get_screen_size,
    .get_text_width = grub_text_region_get_text_width,
    .get_text_height = grub_text_region_get_text_height,
    .map_color = grub_text_region_map_color,
    .update_rect = grub_text_region_update_rect,
    .update_text = grub_text_region_update_text,
    .hide_cursor = grub_text_region_hide_cursor,
    .draw_cursor = grub_text_region_draw_cursor,
  };

GRUB_MOD_INIT(textmenu)
{
  term = grub_term_get_current_output ();
  grub_menu_region_register ("text", &grub_text_region);
}

GRUB_MOD_FINI(textmenu)
{
  grub_menu_region_unregister (&grub_text_region);
}
