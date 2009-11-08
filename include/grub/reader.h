/* reader.h - prototypes for command line reader.  */
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

#ifndef GRUB_READER_HEADER
#define GRUB_READER_HEADER	1

#include <grub/types.h>
#include <grub/err.h>
#include <grub/handler.h>

typedef grub_err_t (*grub_reader_getline_t) (char **, int);

grub_err_t grub_reader_loop (grub_reader_getline_t getline);

void grub_rescue_reader (void);

#endif /* ! GRUB_READER_HEADER */
