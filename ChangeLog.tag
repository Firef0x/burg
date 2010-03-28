2010-01-16  Vladimir Serbinenko  <phcoder@gmail.com>

	Multiboot2 tag support

	* conf/i386.rmk (multiboot2_mod_SOURCES): Replace
	loader/i386/multiboot_mbi.c with loader/i386/multiboot_mbi2.c.
	Remove loader/multiboot_loader.c.
	* include/grub/i386/multiboot.h (grub_multiboot_real_boot): Removed.
	(grub_multiboot2_real_boot): Likewise.
	* include/grub/multiboot.h (grub_multiboot_set_accepts_video): Removed.
	(grub_get_multiboot_mmap_count): New proto.
	(grub_fill_multiboot_mmap): Likewise.
	(grub_multiboot_set_video_mode): Likewise.
	(grub_multiboot_set_console): Likewise.
	(grub_multiboot_load): Likewise.
	(grub_multiboot_load_elf): Likewise.
	(GRUB_MULTIBOOT_CONSOLE_EGA_TEXT): New definition.
	(GRUB_MULTIBOOT_CONSOLE_FRAMEBUFFER): Likewise.
	* include/multiboot.h: Resynced with specification.
	* include/multiboot2.h: Resynced with specification.
	* loader/i386/multiboot_mbi.c (DEFAULT_VIDEO_MODE): Moved from here...
	* loader/i386/multiboot.c (DEFAULT_VIDEO_MODE): ... here.
	* loader/i386/multiboot_mbi.c (HAS_VGA_TEXT): Moved from here ..
	* include/grub/multiboot.h (GRUB_MACHINE_HAS_VGA_TEXT): ... here. All
	users updated.
	* loader/i386/multiboot_mbi.c (accepts_video): Moved from here...
	* loader/i386/multiboot.c (accepts_video): ... here. All users updated.
	* loader/i386/multiboot_mbi.c (grub_multiboot_set_accepts_video):
	Removed.
	* loader/i386/multiboot_mbi.c (grub_get_multiboot_mmap_len):
	Moved from here...
	* loader/i386/multiboot.c (grub_get_multiboot_mmap_len): ... here.
	* loader/i386/multiboot_mbi.c (grub_fill_multiboot_mmap):
	Moved from here...
	* loader/i386/multiboot.c (grub_fill_multiboot_mmap): ... here.
	* loader/i386/multiboot_mbi.c (set_video_mode): Moved from here...
	* loader/i386/multiboot.c (grub_multiboot_set_video_mode): ... here.
	All users updated.
	* loader/i386/multiboot_mbi2.c: New file.
