/* gui_list.c - GUI component to display a selectable list of items.  */
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
#include <grub/gui_string_util.h>
#include <grub/gfxmenu_view.h>
#include <grub/gfxwidgets.h>

struct grub_gui_list_impl
{
  struct grub_gui_list list;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;

  int icon_width;
  int icon_height;
  int item_height;
  int item_padding;
  int item_icon_space;
  int item_spacing;
  grub_font_t item_font;
  grub_font_t selected_item_font;
  grub_gui_color_t item_color;
  int selected_item_color_set;
  grub_gui_color_t selected_item_color;

  int draw_scrollbar;
  int need_to_recreate_scrollbar;
  char *scrollbar_frame_pattern;
  char *scrollbar_thumb_pattern;
  grub_gfxmenu_box_t scrollbar_frame;
  grub_gfxmenu_box_t scrollbar_thumb;
  int scrollbar_width;

  int min_items_shown;
  int max_items_shown;
  int first_shown_index;

  int need_to_recreate_boxes;
  char *theme_dir;
  char *menu_box_pattern;
  char *selected_item_box_pattern;
  grub_gfxmenu_box_t menu_box;
  grub_gfxmenu_box_t selected_item_box;

  grub_gfxmenu_icon_manager_t icon_manager;
  grub_gfxmenu_model_t menu;
};

typedef struct grub_gui_list_impl *list_impl_t;

static void
list_destroy (void *vself)
{
  list_impl_t self = vself;

  grub_free (self->theme_dir);
  grub_free (self->menu_box_pattern);
  grub_free (self->selected_item_box_pattern);
  if (self->menu_box)
    self->menu_box->destroy (self->menu_box);
  if (self->selected_item_box)
    self->selected_item_box->destroy (self->selected_item_box);
  if (self->icon_manager)
    grub_gfxmenu_icon_manager_destroy (self->icon_manager);

  grub_free (self);
}

static int
get_num_shown_items (list_impl_t self)
{
  int n = grub_gfxmenu_model_get_num_entries (self->menu);
  if (self->min_items_shown != -1 && n < self->min_items_shown)
    n = self->min_items_shown;
  if (self->max_items_shown != -1 && n > self->max_items_shown)
    n = self->max_items_shown;
  return n;
}

static int
check_boxes (list_impl_t self)
{
  if (self->need_to_recreate_boxes)
    {
      grub_gui_recreate_box (&self->menu_box,
                             self->menu_box_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->selected_item_box,
                             self->selected_item_box_pattern,
                             self->theme_dir);

      self->need_to_recreate_boxes = 0;
    }

  return (self->menu_box != 0 && self->selected_item_box != 0);
}

static int
check_scrollbar (list_impl_t self)
{
  if (self->need_to_recreate_scrollbar)
    {
      grub_gui_recreate_box (&self->scrollbar_frame,
                             self->scrollbar_frame_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->scrollbar_thumb,
                             self->scrollbar_thumb_pattern,
                             self->theme_dir);

      self->need_to_recreate_scrollbar = 0;
    }

  return (self->scrollbar_frame != 0 && self->scrollbar_thumb != 0);
}

static const char *
list_get_id (void *vself)
{
  list_impl_t self = vself;
  return self->id;
}

static int
list_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return (grub_strcmp (type, "component") == 0
          || grub_strcmp (type, "list") == 0);
}

static struct grub_video_bitmap *
get_item_icon (list_impl_t self, int item_index)
{
  grub_menu_entry_t entry;
  entry = grub_gfxmenu_model_get_entry (self->menu, item_index);
  if (! entry)
    return 0;

  return grub_gfxmenu_icon_manager_get_icon (self->icon_manager, entry);
}

static void
make_selected_item_visible (list_impl_t self)
{
  int selected_index = grub_gfxmenu_model_get_selected_index (self->menu);
  if (selected_index < 0)
    return;   /* No item is selected.  */
  int num_shown_items = get_num_shown_items (self);
  int last_shown_index = self->first_shown_index + (num_shown_items - 1);
  if (selected_index < self->first_shown_index)
    self->first_shown_index = selected_index;
  else if (selected_index > last_shown_index)
    self->first_shown_index = selected_index - (num_shown_items - 1);
}

/* Draw a scrollbar on the menu.  */
static void
draw_scrollbar (list_impl_t self,
                int value, int extent, int min, int max,
                int rightx, int topy, int height)
{
  grub_gfxmenu_box_t frame = self->scrollbar_frame;
  grub_gfxmenu_box_t thumb = self->scrollbar_thumb;
  int frame_vertical_pad = (frame->get_top_pad (frame)
                            + frame->get_bottom_pad (frame));
  int frame_horizontal_pad = (frame->get_left_pad (frame)
                              + frame->get_right_pad (frame));
  int tracktop = topy + frame->get_top_pad (frame);
  int tracklen = height - frame_vertical_pad;
  frame->set_content_size (frame, self->scrollbar_width, tracklen);
  int thumby = tracktop + tracklen * (value - min) / (max - min);
  int thumbheight = tracklen * extent / (max - min) + 1;
  thumb->set_content_size (thumb,
                           self->scrollbar_width - frame_horizontal_pad,
                           thumbheight - (thumb->get_top_pad (thumb)
                                          + thumb->get_bottom_pad (thumb)));
  frame->draw (frame,
               rightx - (self->scrollbar_width + frame_horizontal_pad),
               topy);
  thumb->draw (thumb,
               rightx - (self->scrollbar_width - frame->get_right_pad (frame)),
               thumby);
}

/* Draw the list of items.  */
static void
draw_menu (list_impl_t self)
{
  if (! self->menu_box || ! self->selected_item_box)
    return;

  int boxpad = self->item_padding;
  int icon_text_space = self->item_icon_space;
  int item_vspace = self->item_spacing;

  int ascent = grub_font_get_ascent (self->item_font);
  int descent = grub_font_get_descent (self->item_font);
  int item_height = self->item_height;

  int total_num_items = grub_gfxmenu_model_get_num_entries (self->menu);
  int num_shown_items = get_num_shown_items (self);
  grub_gfxmenu_box_t box = self->menu_box;
  int width = self->bounds.width;
  int height = self->bounds.height;

  int box_left_pad = box->get_left_pad (box);
  int box_top_pad = box->get_top_pad (box);
  int box_right_pad = box->get_right_pad (box);
  int box_bottom_pad = box->get_bottom_pad (box);

  box->set_content_size (box,
                         width - box_left_pad - box_right_pad,
                         height - box_top_pad - box_bottom_pad);

  box->draw (box, 0, 0);

  make_selected_item_visible (self);

  int drawing_scrollbar = (self->draw_scrollbar
                           && (num_shown_items < total_num_items)
                           && check_scrollbar (self));

  int scrollbar_h_space = drawing_scrollbar ? self->scrollbar_width : 0;

  int item_top = box_top_pad + boxpad;
  int item_left = box_left_pad + boxpad;
  int menu_index;
  int visible_index;

  for (visible_index = 0, menu_index = self->first_shown_index;
       visible_index < num_shown_items && menu_index < total_num_items;
       visible_index++, menu_index++)
    {
      int is_selected =
        (menu_index == grub_gfxmenu_model_get_selected_index (self->menu));

      if (is_selected)
        {
          grub_gfxmenu_box_t selbox = self->selected_item_box;
          int sel_leftpad = selbox->get_left_pad (selbox);
          int sel_toppad = selbox->get_top_pad (selbox);
          selbox->set_content_size (selbox,
                                    (width - 2 * boxpad
                                     - box_left_pad - box_right_pad
                                     - scrollbar_h_space),
                                    item_height);
          selbox->draw (selbox,
                        item_left - sel_leftpad,
                        item_top - sel_toppad);
        }

      struct grub_video_bitmap *icon;
      if ((icon = get_item_icon (self, menu_index)) != 0)
        grub_video_blit_bitmap (icon, GRUB_VIDEO_BLIT_BLEND,
                                item_left,
                                item_top + (item_height - self->icon_height) / 2,
                                0, 0, self->icon_width, self->icon_height);

      const char *item_title =
        grub_gfxmenu_model_get_entry_title (self->menu, menu_index);
      grub_font_t font =
        (is_selected && self->selected_item_font
         ? self->selected_item_font
         : self->item_font);
      grub_gui_color_t text_color =
        ((is_selected && self->selected_item_color_set)
         ? self->selected_item_color
         : self->item_color);
      grub_font_draw_string (item_title,
                             font,
                             grub_gui_map_color (text_color),
                             item_left + self->icon_width + icon_text_space,
                             (item_top + (item_height - (ascent + descent))
                              / 2 + ascent));

      item_top += item_height + item_vspace;
    }

  if (drawing_scrollbar)
    draw_scrollbar (self,
                    self->first_shown_index, num_shown_items,
                    0, total_num_items,
                    width - box_right_pad + self->scrollbar_width,
                    box_top_pad + boxpad,
                    height - box_top_pad - box_bottom_pad);
}

static void
list_paint (void *vself, const grub_video_rect_t *region)
{
  list_impl_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  check_boxes (self);

  grub_gui_set_viewport (&self->bounds, &vpsave);
  draw_menu (self);
  grub_gui_restore_viewport (&vpsave);
}

static void
list_set_parent (void *vself, grub_gui_container_t parent)
{
  list_impl_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
list_get_parent (void *vself)
{
  list_impl_t self = vself;
  return self->parent;
}

static void
list_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  list_impl_t self = vself;
  self->bounds = *bounds;
}

static void
list_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  list_impl_t self = vself;
  *bounds = self->bounds;
}

static void
list_get_minimal_size (void *vself, unsigned *width, unsigned *height)
{
  list_impl_t self = vself;

  if (check_boxes (self))
    {
      int boxpad = self->item_padding;
      int item_vspace = self->item_spacing;
      int item_height = self->item_height;
      int num_items = get_num_shown_items (self);

      grub_gfxmenu_box_t box = self->menu_box;
      int box_left_pad = box->get_left_pad (box);
      int box_top_pad = box->get_top_pad (box);
      int box_right_pad = box->get_right_pad (box);
      int box_bottom_pad = box->get_bottom_pad (box);

      *width = 400 + 2 * boxpad + box_left_pad + box_right_pad;

      /* Set the menu box height to fit the items.  */
      *height = (item_height * num_items
                 + item_vspace * (num_items - 1)
                 + 2 * boxpad
                 + box_top_pad + box_bottom_pad);
    }
  else
    {
      *width = 0;
      *height = 0;
    }
}

static grub_err_t
list_set_property (void *vself, const char *name, const char *value)
{
  list_impl_t self = vself;
  if (grub_strcmp (name, "item_font") == 0)
    {
      self->item_font = grub_font_get (value);
    }
  else if (grub_strcmp (name, "selected_item_font") == 0)
    {
      if (! value || grub_strcmp (value, "inherit") == 0)
        self->selected_item_font = 0;
      else
        self->selected_item_font = grub_font_get (value);
    }
  else if (grub_strcmp (name, "item_color") == 0)
    {
      grub_gui_parse_color (value, &self->item_color);
    }
  else if (grub_strcmp (name, "selected_item_color") == 0)
    {
      if (! value || grub_strcmp (value, "inherit") == 0)
        {
          self->selected_item_color_set = 0;
        }
      else
        {
          if (grub_gui_parse_color (value, &self->selected_item_color)
              == GRUB_ERR_NONE)
            self->selected_item_color_set = 1;
        }
    }
  else if (grub_strcmp (name, "icon_width") == 0)
    {
      self->icon_width = grub_strtol (value, 0, 10);
      grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                               self->icon_width,
                                               self->icon_height);
    }
  else if (grub_strcmp (name, "icon_height") == 0)
    {
      self->icon_height = grub_strtol (value, 0, 10);
      grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                               self->icon_width,
                                               self->icon_height);
    }
  else if (grub_strcmp (name, "item_height") == 0)
    {
      self->item_height = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_padding") == 0)
    {
      self->item_padding = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_icon_space") == 0)
    {
      self->item_icon_space = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_spacing") == 0)
    {
      self->item_spacing = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "visible") == 0)
    {
      self->visible = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "menu_pixmap_style") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->menu_box_pattern);
      self->menu_box_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "selected_item_pixmap_style") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->selected_item_box_pattern);
      self->selected_item_box_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "scrollbar_frame") == 0)
    {
      self->need_to_recreate_scrollbar = 1;
      grub_free (self->scrollbar_frame_pattern);
      self->scrollbar_frame_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "scrollbar_thumb") == 0)
    {
      self->need_to_recreate_scrollbar = 1;
      grub_free (self->scrollbar_thumb_pattern);
      self->scrollbar_thumb_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "scrollbar_width") == 0)
    {
      self->scrollbar_width = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar") == 0)
    {
      self->draw_scrollbar = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "min_items_shown") == 0)
    {
      self->min_items_shown = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "max_items_shown") == 0)
    {
      self->max_items_shown = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "theme_dir") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->theme_dir);
      self->theme_dir = value ? grub_strdup (value) : 0;
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

/* Set necessary information that the gfxmenu view provides.  */
static void
list_set_view_info (void *vself,
                    const char *theme_path,
                    grub_gfxmenu_model_t menu)
{
  list_impl_t self = vself;
  grub_gfxmenu_icon_manager_set_theme_path (self->icon_manager, theme_path);
  self->menu = menu;
}

static struct grub_gui_component_ops list_comp_ops =
  {
    .destroy = list_destroy,
    .get_id = list_get_id,
    .is_instance = list_is_instance,
    .paint = list_paint,
    .set_parent = list_set_parent,
    .get_parent = list_get_parent,
    .set_bounds = list_set_bounds,
    .get_bounds = list_get_bounds,
    .get_minimal_size = list_get_minimal_size,
    .set_property = list_set_property
  };

static struct grub_gui_list_ops list_ops =
{
  .set_view_info = list_set_view_info
};

grub_gui_component_t
grub_gui_list_new (void)
{
  list_impl_t self;
  grub_font_t default_font;
  grub_gui_color_t default_fg_color;
  grub_gui_color_t default_bg_color;

  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->list.ops = &list_ops;
  self->list.component.ops = &list_comp_ops;

  self->visible = 1;

  default_font = grub_font_get ("Helvetica 12");
  default_fg_color = grub_gui_color_rgb (0, 0, 0);
  default_bg_color = grub_gui_color_rgb (255, 255, 255);

  self->icon_width = 32;
  self->icon_height = 32;
  self->item_height = 42;
  self->item_padding = 14;
  self->item_icon_space = 4;
  self->item_spacing = 16;
  self->item_font = default_font;
  self->selected_item_font = 0;    /* Default to using the item_font.  */
  self->item_color = default_fg_color;
  self->selected_item_color_set = 0;  /* Default to using the item_color.  */
  self->selected_item_color = default_fg_color;

  self->draw_scrollbar = 1;
  self->need_to_recreate_scrollbar = 1;
  self->scrollbar_frame = 0;
  self->scrollbar_thumb = 0;
  self->scrollbar_frame_pattern = 0;
  self->scrollbar_thumb_pattern = 0;
  self->scrollbar_width = 16;

  self->min_items_shown = -1;
  self->max_items_shown = -1;
  self->first_shown_index = 0;

  self->need_to_recreate_boxes = 0;
  self->theme_dir = 0;
  self->menu_box_pattern = 0;
  self->selected_item_box_pattern = 0;
  self->menu_box = grub_gfxmenu_create_box (0, 0);
  self->selected_item_box = grub_gfxmenu_create_box (0, 0);

  self->icon_manager = grub_gfxmenu_icon_manager_new ();
  if (! self->icon_manager)
    {
      self->list.component.ops->destroy (self);
      return 0;
    }
  grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                           self->icon_width,
                                           self->icon_height);
  return (grub_gui_component_t) self;
}
