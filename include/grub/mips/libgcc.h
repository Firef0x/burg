/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007,2009  Free Software Foundation, Inc.
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

#include <config.h>

#ifdef HAVE___ASHLDI3
void EXPORT_FUNC (__ashldi3) (void);
#endif
#ifdef HAVE___ASHRDI3
void EXPORT_FUNC (__ashrdi3) (void);
#endif
#ifdef HAVE___LSHRDI3
void EXPORT_FUNC (__lshrdi3) (void);
#endif
#ifdef HAVE___UCMPDI2
void EXPORT_FUNC (__ucmpdi2) (void);
#endif
#ifdef HAVE___BSWAPSI2
void EXPORT_FUNC (__bswapsi2) (void);
#endif
#ifdef HAVE___BSWAPDI2
void EXPORT_FUNC (__bswapdi2) (void);
#endif
