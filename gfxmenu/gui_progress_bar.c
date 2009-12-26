/* gui_progress_bar.c - GUI progress bar component.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
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
#include <grub/gui.h>
#include <grub/font.h>
#include <grub/gui_string_util.h>
#include <grub/gfxmenu_view.h>
#include <grub/gfxwidgets.h>

struct grub_gui_progress_bar
{
  struct grub_gui_component_ops *progress_bar;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int preferred_width;
  int preferred_height;
  int visible;
  int start;
  int end;
  int value;
  int show_text;
  char *text;
  grub_font_t font;
  grub_gui_color_t text_color;
  grub_gui_color_t border_color;
  grub_gui_color_t bg_color;
  grub_gui_color_t fg_color;

  char *theme_dir;
  int need_to_recreate_pixmaps;
  char *bar_pattern;
  char *highlight_pattern;
  grub_gfxmenu_box_t bar_box;
  grub_gfxmenu_box_t highlight_box;
};

typedef struct grub_gui_progress_bar *grub_gui_progress_bar_t;

static void
progress_bar_destroy (void *vself)
{
  grub_gui_progress_bar_t self = vself;
  grub_free (self);
}

static const char *
progress_bar_get_id (void *vself)
{
  grub_gui_progress_bar_t self = vself;
  return self->id;
}

static int
progress_bar_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return grub_strcmp (type, "component") == 0;
}

static int
check_pixmaps (grub_gui_progress_bar_t self)
{
  if (self->need_to_recreate_pixmaps)
    {
      grub_gui_recreate_box (&self->bar_box,
                             self->bar_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->highlight_box,
                             self->highlight_pattern,
                             self->theme_dir);

      self->need_to_recreate_pixmaps = 0;
    }

  return (self->bar_box != 0 && self->highlight_box != 0);
}

static void
draw_filled_rect_bar (grub_gui_progress_bar_t self)
{
  /* Set the progress bar's frame.  */
  grub_video_rect_t f;
  f.x = 1;
  f.y = 1;
  f.width = self->bounds.width - 2;
  f.height = self->bounds.height - 2;

  /* Border.  */
  grub_video_fill_rect (grub_gui_map_color (self->border_color),
                        f.x - 1, f.y - 1,
                        f.width + 2, f.height + 2);

  /* Bar background.  */
  int barwidth = (f.width
                  * (self->value - self->start)
                  / (self->end - self->start));
  grub_video_fill_rect (grub_gui_map_color (self->bg_color),
                        f.x + barwidth, f.y,
                        f.width - barwidth, f.height);

  /* Bar foreground.  */
  grub_video_fill_rect (grub_gui_map_color (self->fg_color),
                        f.x, f.y,
                        barwidth, f.height);
}

static void
draw_pixmap_bar (grub_gui_progress_bar_t self)
{
  grub_gfxmenu_box_t bar = self->bar_box;
  grub_gfxmenu_box_t hl = self->highlight_box;
  int w = self->bounds.width;
  int h = self->bounds.height;
  int bar_l_pad = bar->get_left_pad (bar);
  int bar_r_pad = bar->get_right_pad (bar);
  int bar_t_pad = bar->get_top_pad (bar);
  int bar_b_pad = bar->get_bottom_pad (bar);
  int bar_h_pad = bar_l_pad + bar_r_pad;
  int bar_v_pad = bar_t_pad + bar_b_pad;
  int tracklen = w - bar_h_pad;
  int trackheight = h - bar_v_pad;
  int barwidth;

  if (self->end == self->start)
    return;

  bar->set_content_size (bar, tracklen, trackheight);

  barwidth = (tracklen * (self->value - self->start) 
	      / (self->end - self->start));

  hl->set_content_size (hl, barwidth, h - bar_v_pad);

  bar->draw (bar, 0, 0);
  hl->draw (hl, bar_l_pad, bar_t_pad);
}

static void
draw_text (grub_gui_progress_bar_t self)
{
  const char *text = self->text;
  if (text && self->show_text)
    {
      grub_font_t font = self->font;
      grub_video_color_t text_color = grub_gui_map_color (self->text_color);
      int width = self->bounds.width;
      int height = self->bounds.height;

      /* Center the text. */
      int text_width = grub_font_get_string_width (font, text);
      int x = (width - text_width) / 2;
      int y = ((height - grub_font_get_descent (font)) / 2
               + grub_font_get_ascent (font) / 2);
      grub_font_draw_string (text, font, text_color, x, y);
    }
}

static void
progress_bar_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_progress_bar_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  grub_gui_set_viewport (&self->bounds, &vpsave);

  if (check_pixmaps (self))
    draw_pixmap_bar (self);
  else
    draw_filled_rect_bar (self);

  draw_text (self);

  grub_gui_restore_viewport (&vpsave);
}

static void
progress_bar_set_parent (void *vself, grub_gui_container_t parent)
{
  grub_gui_progress_bar_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
progress_bar_get_parent (void *vself)
{
  grub_gui_progress_bar_t self = vself;
  return self->parent;
}

static void
progress_bar_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  grub_gui_progress_bar_t self = vself;
  self->bounds = *bounds;
}

static void
progress_bar_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  grub_gui_progress_bar_t self = vself;
  *bounds = self->bounds;
}

static void
progress_bar_get_preferred_size (void *vself, int *width, int *height)
{
  grub_gui_progress_bar_t self = vself;

  *width = 200;
  *height = 28;

  /* Allow preferred dimensions to override the progress_bar dimensions.  */
  if (self->preferred_width >= 0)
    *width = self->preferred_width;
  if (self->preferred_height >= 0)
    *height = self->preferred_height;
}

static grub_err_t
progress_bar_set_property (void *vself, const char *name, const char *value)
{
  grub_gui_progress_bar_t self = vself;
  if (grub_strcmp (name, "value") == 0)
    {
      self->value = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "start") == 0)
    {
      self->start = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "end") == 0)
    {
      self->end = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "text") == 0)
    {
      grub_free (self->text);
      if (! value)
        value = "";
      self->text = grub_strdup (value);
    }
  else if (grub_strcmp (name, "font") == 0)
    {
      self->font = grub_font_get (value);
    }
  else if (grub_strcmp (name, "text_color") == 0)
    {
      grub_gui_parse_color (value, &self->text_color);
    }
  else if (grub_strcmp (name, "border_color") == 0)
    {
       grub_gui_parse_color (value, &self->border_color);
    }
  else if (grub_strcmp (name, "bg_color") == 0)
    {
       grub_gui_parse_color (value, &self->bg_color);
    }
  else if (grub_strcmp (name, "fg_color") == 0)
    {
      grub_gui_parse_color (value, &self->fg_color);
    }
  else if (grub_strcmp (name, "bar_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      grub_free (self->bar_pattern);
      self->bar_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "highlight_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      grub_free (self->highlight_pattern);
      self->highlight_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "theme_dir") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      grub_free (self->theme_dir);
      self->theme_dir = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "preferred_size") == 0)
    {
      int w;
      int h;
      if (grub_gui_parse_2_tuple (value, &w, &h) != GRUB_ERR_NONE)
        return grub_errno;
      self->preferred_width = w;
      self->preferred_height = h;
    }
  else if (grub_strcmp (name, "visible") == 0)
    {
      self->visible = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "show_text") == 0)
    {
      self->show_text = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_free (self->id);
      if (value)
        self->id = grub_strdup (value);
      else
        self->id = 0;
    }
  return grub_errno;
}

static struct grub_gui_component_ops progress_bar_ops =
{
  .destroy = progress_bar_destroy,
  .get_id = progress_bar_get_id,
  .is_instance = progress_bar_is_instance,
  .paint = progress_bar_paint,
  .set_parent = progress_bar_set_parent,
  .get_parent = progress_bar_get_parent,
  .set_bounds = progress_bar_set_bounds,
  .get_bounds = progress_bar_get_bounds,
  .get_preferred_size = progress_bar_get_preferred_size,
  .set_property = progress_bar_set_property
};

grub_gui_component_t
grub_gui_progress_bar_new (void)
{
  grub_gui_progress_bar_t self;
  self = grub_malloc (sizeof (*self));
  if (! self)
    return 0;
  self->progress_bar = &progress_bar_ops;
  self->parent = 0;
  self->bounds.x = 0;
  self->bounds.y = 0;
  self->bounds.width = 0;
  self->bounds.height = 0;
  self->id = 0;
  self->preferred_width = -1;
  self->preferred_height = -1;
  self->visible = 1;
  self->start = 0;
  self->end = 0;
  self->value = 0;
  self->show_text = 1;
  self->text = grub_strdup ("");
  self->font = grub_font_get ("Helvetica 10");
  grub_gui_color_t black = { .red = 0, .green = 0, .blue = 0, .alpha = 255 };
  grub_gui_color_t gray = { .red = 128, .green = 128, .blue = 128, .alpha = 255 };
  grub_gui_color_t lightgray = { .red = 200, .green = 200, .blue = 200, .alpha = 255 };
  self->text_color = black;
  self->border_color = black;
  self->bg_color = gray;
  self->fg_color = lightgray;

  self->theme_dir = 0;
  self->need_to_recreate_pixmaps = 0;
  self->bar_pattern = 0;
  self->highlight_pattern = 0;
  self->bar_box = 0;
  self->highlight_box = 0;

  return (grub_gui_component_t) self;
}
