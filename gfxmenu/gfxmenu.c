/* gfxmenu.c - Graphical menu interface controller. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/command.h>
#include <grub/video.h>
#include <grub/gfxterm.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/normal.h>
#include <grub/gfxwidgets.h>
#include <grub/menu.h>
#include <grub/menu_viewer.h>
#include <grub/gfxmenu_model.h>
#include <grub/gfxmenu_view.h>
#include <grub/time.h>

grub_gfxmenu_view_t cached_view;

static grub_err_t
set_graphics_mode (void)
{
  const char *modestr = grub_env_get ("gfxmode");
  if (!modestr || !modestr[0])
    modestr = "auto";
  return grub_video_set_mode (modestr, GRUB_VIDEO_MODE_TYPE_PURE_TEXT, 0);
}

/* FIXME: conflicts with gfxterm.  */
static grub_err_t
set_text_mode (void)
{
  return grub_video_restore ();
}

void 
grub_gfxmenu_viewer_fini (void *data __attribute__ ((unused)))
{
  set_text_mode ();
}

/* FIXME: 't' and 'c'.  */
grub_err_t
grub_gfxmenu_try (int entry, grub_menu_t menu, int nested)
{
  grub_gfxmenu_view_t view = NULL;
  const char *theme_path;
  struct grub_menu_viewer *instance;
  grub_err_t err;
  struct grub_video_mode_info mode_info;

  theme_path = grub_env_get ("theme");
  if (! theme_path)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, "no theme specified");

  instance = grub_zalloc (sizeof (*instance));
  if (!instance)
    return grub_errno;

  set_graphics_mode ();
  err = grub_video_get_info (&mode_info);
  if (err)
    {
      set_text_mode ();
      return 0;
    }

  if (!cached_view || grub_strcmp (cached_view->theme_path, theme_path) != 0
      || cached_view->screen.width != (int) mode_info.width
      || cached_view->screen.height != (int) mode_info.height)
    {
      grub_free (cached_view);
      /* Create the view.  */
      cached_view = grub_gfxmenu_view_new (theme_path, mode_info.width,
					   mode_info.height);
    }

  if (! cached_view)
    {
      grub_free (instance);
      set_text_mode ();
      return grub_errno;
    }

  view = cached_view;

  view->double_repaint = (mode_info.mode_type
			  & GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED)
    && !(mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_UPDATING_SWAP);
  view->selected = entry;
  view->menu = menu;
  view->nested = nested;
  view->first_timeout = -1;

  grub_gfxmenu_view_draw (view);

  instance->data = view;
  instance->set_chosen_entry = grub_gfxmenu_set_chosen_entry;
  instance->fini = grub_gfxmenu_viewer_fini;
  instance->print_timeout = grub_gfxmenu_print_timeout;
  instance->clear_timeout = grub_gfxmenu_clear_timeout;

  grub_menu_register_viewer (instance);

  return GRUB_ERR_NONE;
}

GRUB_MOD_INIT (gfxmenu)
{
  grub_gfxmenu_try_hook = grub_gfxmenu_try;
}

GRUB_MOD_FINI (gfxmenu)
{
  grub_gfxmenu_view_destroy (cached_view);
  grub_gfxmenu_try_hook = NULL;
}
