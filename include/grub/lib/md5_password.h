/* md5.h - an implementation of the MD5 algorithm and MD5 crypt */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef GRUB_MD5_PASSWORD_H
#define GRUB_MD5_PASSWORD_H	1

/* If CHECK is true, check a password for correctness. Returns 0
   if password was correct, and a value != 0 for error, similarly
   to strcmp.
   If CHECK is false, crypt KEY and save the result in CRYPTED.
   CRYPTED must have a salt.  */
extern int grub_md5_password (const char *key, char *crypted, int check);

/* For convenience.  */
#define grub_check_md5_password(key,crypted)	\
  grub_md5_password((key), (crypted), 1)
#define grub_make_md5_password(key,crypted)	\
  grub_md5_password((key), (crypted), 0)

#endif
