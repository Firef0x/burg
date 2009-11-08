/* startup.S - Startup code for the PowerPC.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2008  Free Software Foundation, Inc.
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

#include <grub/symbol.h>
#include <grub/cpu/kernel.h>

	.text
	.align	2

	.globl	_start, grub_code_start
_start:
grub_code_start:
	b	LOCAL(codestart)

	. = _start + GRUB_KERNEL_CPU_PREFIX

VARIABLE(grub_prefix)
	/* to be filled by grub-mkelfimage */

	/*
	 *  Leave some breathing room for the prefix.
	 */

	. = _start + GRUB_KERNEL_CPU_DATA_END

LOCAL(codestart):

#ifdef APPLE_CC

	li      r2, 0
	li      r13, 0

	/* Stage1 won't zero BSS for us. In other cases, why not do it again?  */
	lis	r6, hi16(grub_bss_start - 4)
	ori	r6, r6, lo16(grub_bss_start - 4)
	lis	r7, hi16(grub_bss_end - 4)
	ori	r7, r7, lo16(grub_bss_end - 4)
	subf	r7, r6, r7
	srwi	r7, r7, 2 /* We store 4 bytes at a time.  */
	mtctr	r7
2:	stwu	r2, 4(r6) /* We know r2 is already 0 from above.  */
	bdnz	2b

	/* Store r5 in grub_ieee1275_entry_fn.  */
	lis	r9, ha16(grub_ieee1275_entry_fn)
	stw	r5, lo16(grub_ieee1275_entry_fn)(r9)

	bl	grub_main
1:	b	1b

#else

	li      2, 0
	li      13, 0

	/* Stage1 won't zero BSS for us. In other cases, why not do it again?  */
	lis	6, (grub_bss_start - 4)@h
	ori	6, 6, (grub_bss_start - 4)@l
	lis	7, (grub_bss_end - 4)@h
	ori	7, 7, (grub_bss_end - 4)@l
	subf	7, 6, 7
	srwi	7, 7, 2 /* We store 4 bytes at a time.  */
	mtctr	7
2:	stwu	2, 4(6) /* We know r2 is already 0 from above.  */
	bdnz	2b

	/* Store r5 in grub_ieee1275_entry_fn.  */
	lis	9, grub_ieee1275_entry_fn@ha
	stw	5, grub_ieee1275_entry_fn@l(9)

	bl	grub_main
1:	b	1b

#endif
