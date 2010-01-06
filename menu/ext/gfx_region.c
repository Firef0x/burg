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
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/menu_region.h>

#define DEFAULT_VIDEO_MODE "auto"

static int screen_width;
static int screen_height;
static grub_font_t default_font;
static struct grub_menu_region grub_gfx_region;

#define REFERENCE_STRING	"m"

static int
video_hook (grub_video_adapter_t p __attribute__ ((unused)),
	    struct grub_video_mode_info *info,
	    void *closure __attribute__ ((unused)))
{
  return ! (info->mode_type & GRUB_VIDEO_MODE_TYPE_PURE_TEXT);
}

static grub_err_t
grub_gfx_region_init (void)
{
  const char *modevar;
  struct grub_video_mode_info mode_info;
  grub_err_t err;
  char *fn;

  modevar = grub_env_get ("gfxmode");
  if (! modevar || *modevar == 0)
    err = grub_video_set_mode (DEFAULT_VIDEO_MODE, video_hook, 0);
  else
    {
      char *tmp;

      tmp = grub_malloc (grub_strlen (modevar)
			 + sizeof (DEFAULT_VIDEO_MODE) + 1);
      if (! tmp)
	return grub_errno;
      grub_sprintf (tmp, "%s;" DEFAULT_VIDEO_MODE, modevar);
      err = grub_video_set_mode (tmp, video_hook, 0);
      grub_free (tmp);
    }

  if (err)
    return err;

  err = grub_video_get_info (&mode_info);
  if (err)
    return err;

  screen_width = mode_info.width;
  screen_height = mode_info.height;

  fn = grub_env_get ("gfxfont");
  if (! fn)
    fn = "";

  default_font = grub_font_get (fn);
  grub_gfx_region.char_width = grub_font_get_string_width (default_font,
							   REFERENCE_STRING);
  grub_gfx_region.char_height = grub_font_get_ascent (default_font) +
    grub_font_get_descent (default_font);

  if ((! grub_gfx_region.char_width) || (! grub_gfx_region.char_height))
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "invalid font");

  return grub_errno;
}

static grub_err_t
grub_gfx_region_fini (void)
{
  grub_video_restore ();
  return (grub_errno = GRUB_ERR_NONE);
}

static void
grub_gfx_region_get_screen_size (int *width, int *height)
{
  *width = screen_width;
  *height = screen_height;
}

static grub_font_t
grub_gfx_region_get_font (const char *name)
{
  return (name) ? grub_font_get (name) : default_font;
}

static int
grub_gfx_region_get_text_width (grub_font_t font, const char *str,
				int count, int *chars)
{
  int width, w;
  const char *p;

  if (! count)
    {
      if (chars)
	*chars = grub_strlen (str);

      return grub_font_get_string_width (font, str);
    }

  width = 0;
  p = str;
  while ((count) && (*p) &&
	 ((w = grub_font_get_code_width (font, p, &p)) > 0))
    {
      width += w;
      count--;
    }

  if (chars)
    *chars = p - str;

  return width;
}

static int
grub_gfx_region_get_text_height (grub_font_t font)
{
  return grub_font_get_height (font);
}

static struct grub_video_bitmap *
grub_gfx_region_get_bitmap (const char *name)
{
  struct grub_video_bitmap *bitmap;

  if (grub_video_bitmap_load (&bitmap, name))
    {
      grub_errno = 0;
      return 0;
    }

  return bitmap;
}

static void
grub_gfx_region_free_bitmap (struct grub_video_bitmap *bitmap)
{
  grub_video_bitmap_destroy (bitmap);
}

static void
grub_gfx_region_scale_bitmap (struct grub_menu_region_bitmap *bitmap)
{
  if (bitmap->bitmap)
    {
      grub_video_bitmap_destroy (bitmap->bitmap);
      bitmap->bitmap = 0;
    }

  grub_video_bitmap_create_scaled (&bitmap->bitmap, bitmap->common.width,
				   bitmap->common.height,
				   bitmap->cache->bitmap,
				   bitmap->scale, bitmap->color);
}

static grub_video_color_t
grub_gfx_region_map_color (int fg_color, int bg_color __attribute__ ((unused)))
{
  return grub_video_map_color (fg_color);
}

static grub_video_color_t
grub_gfx_region_map_rgb (grub_uint8_t red, grub_uint8_t green, grub_uint8_t blue)
{
  return grub_video_map_rgb (red, green, blue);
}

static void
grub_gfx_region_update_rect (struct grub_menu_region_rect *rect,
			     int x, int y, int width, int height,
			     int scn_x, int scn_y)
{
  if (rect->fill)
    {
      struct grub_font_glyph *glyph;
      int w, h, font_ascent, font_width;

      grub_video_set_viewport (scn_x, scn_y, width, height);
      glyph = grub_font_get_glyph_with_fallback (default_font, rect->fill);
      font_ascent = grub_font_get_ascent (default_font);
      font_width = glyph->device_width;
      for (h = -y ; h < height; h += grub_gfx_region.char_height)
	{
	  for (w = -x; w < width; w += font_width)
	    grub_font_draw_glyph (glyph, rect->color, w, h + font_ascent);
	}
      grub_video_set_viewport (0, 0, screen_width, screen_height);
    }
  else
    grub_video_fill_rect (rect->color, scn_x, scn_y, width, height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

static void
grub_gfx_region_update_text (struct grub_menu_region_text *text,
			     int x, int y, int width, int height,
			     int scn_x, int scn_y)
{
  grub_video_set_viewport (scn_x, scn_y, width, height);
  grub_font_draw_string (text->text, text->font, text->color, - x,
			 - y + grub_font_get_ascent (text->font));
  grub_video_set_viewport (0, 0, screen_width, screen_height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

static void
grub_gfx_region_update_bitmap (struct grub_menu_region_bitmap *bitmap,
			       int x, int y, int width, int height,
			       int scn_x, int scn_y)
{
  struct grub_video_bitmap *bm;

  bm = (bitmap->bitmap) ? bitmap->bitmap : bitmap->cache->bitmap;
  grub_video_blit_bitmap (bm, GRUB_VIDEO_BLIT_BLEND,
			  scn_x, scn_y, x, y, width, height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

#define CURSOR_HEIGHT	2

static void
grub_gfx_region_draw_cursor (struct grub_menu_region_text *text,
			     int width, int height, int scn_x, int scn_y)
{
  int y;

  y = grub_font_get_ascent (text->font);
  if (y >= height)
    return;

  height -= y;
  if (height > CURSOR_HEIGHT)
    height = CURSOR_HEIGHT;

  grub_video_fill_rect (text->color, scn_x, scn_y + y, width, height);
  grub_video_update_rect (scn_x, scn_y + y, width, height);
}

static struct grub_menu_region grub_gfx_region =
  {
    .name = "gfx",
    .init = grub_gfx_region_init,
    .fini = grub_gfx_region_fini,
    .get_screen_size = grub_gfx_region_get_screen_size,
    .get_font = grub_gfx_region_get_font,
    .get_text_width = grub_gfx_region_get_text_width,
    .get_text_height = grub_gfx_region_get_text_height,
    .get_bitmap = grub_gfx_region_get_bitmap,
    .free_bitmap = grub_gfx_region_free_bitmap,
    .scale_bitmap = grub_gfx_region_scale_bitmap,
    .map_color = grub_gfx_region_map_color,
    .map_rgb = grub_gfx_region_map_rgb,
    .update_rect = grub_gfx_region_update_rect,
    .update_text = grub_gfx_region_update_text,
    .update_bitmap = grub_gfx_region_update_bitmap,
    .draw_cursor = grub_gfx_region_draw_cursor
  };

GRUB_MOD_INIT(gfxmenu)
{
  grub_menu_region_register ("gfx", &grub_gfx_region);
}

GRUB_MOD_FINI(gfxmenu)
{
  grub_menu_region_unregister (&grub_gfx_region);
}
