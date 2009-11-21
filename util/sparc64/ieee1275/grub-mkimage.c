/* grub-mkimage.c - make a bootable image */
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
#include <config.h>
#include <grub/types.h>
#include <grub/machine/boot.h>
#include <grub/machine/kernel.h>
#include <grub/kernel.h>
#include <grub/disk.h>
#include <grub/util/misc.h>
#include <grub/util/resolve.h>
#include <grub/misc.h>
#include <grub/util/obj.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define _GNU_SOURCE	1
#include <getopt.h>

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

static void
compress_kernel (char *kernel_img, size_t kernel_size,
		 char **core_img, size_t *core_size)
{
  /* No compression support yet.  */
  grub_util_info ("kernel_img=%p, kernel_size=0x%x", kernel_img, kernel_size);
  *core_img = xmalloc (kernel_size);
  memcpy (*core_img, kernel_img, kernel_size);
  *core_size = kernel_size;
}

#define LINK_ADDR	0x200000

static void
generate_image (const char *dir, const char *prefix, FILE *out, char *mods[],
		char *memdisk_path, char *config_path, int netboot)
{
  char *kernel_img, *core_img, *boot_img;
  size_t kernel_size, total_module_size, core_size, boot_size;
  char *boot_path;
  unsigned int num;
  struct grub_util_path_list *path_list;
  struct grub_util_obj *obj;
  struct grub_util_obj_segment *modinfo, *seg;

  path_list = grub_util_resolve_dependencies (dir, "moddep.lst", mods);

  obj = xmalloc_zero (sizeof (*obj));
  modinfo = grub_obj_add_modinfo (obj, dir, path_list, 0,
				  memdisk_path, config_path);
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, GRUB_TARGET_MIN_SEG_ALIGN, GRUB_OBJ_MERGE_ALL);
  grub_obj_add_kernel_symbols (obj, modinfo, 0);
  grub_obj_reloc_symbols (obj, GRUB_OBJ_MERGE_ALL);
  grub_obj_link (obj, LINK_ADDR);

  kernel_size = modinfo->segment.offset - LINK_ADDR;
  total_module_size = modinfo->segment.size;
  kernel_img = xmalloc_zero (kernel_size + total_module_size);

  seg = obj->segments;
  while (seg)
    {
      if (seg->segment.type != GRUB_OBJ_SEG_BSS)
	{
	  int offset = seg->segment.offset - LINK_ADDR;

	  memcpy (kernel_img + offset, seg->data, seg->raw_size);
	}
      seg = seg->next;
    }

  grub_obj_free (obj);

  if ((GRUB_KERNEL_MACHINE_PREFIX + strlen (prefix) + 1)
      > GRUB_KERNEL_MACHINE_DATA_END)
    grub_util_error ("prefix too long");
  strcpy (kernel_img + GRUB_KERNEL_MACHINE_PREFIX, prefix);

  compress_kernel (kernel_img, kernel_size + total_module_size,
		   &core_img, &core_size);

  grub_util_info ("the core size is 0x%x", core_size);

  num = ((core_size + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS);
  num <<= GRUB_DISK_SECTOR_BITS;

  boot_path = grub_util_get_path (dir,
				  (netboot) ? "netboot.img" : "diskboot.img");
  boot_size = grub_util_get_image_size (boot_path);
  if (boot_size != GRUB_DISK_SECTOR_SIZE)
    grub_util_error ("diskboot.img/netboot.img is not one sector size");

  boot_img = grub_util_read_image (boot_path);

  /* sparc is a big endian architecture.  */
  if (netboot)
    {
      *((grub_uint32_t *) (boot_img + GRUB_BOOT_MACHINE_CODE_SIZE))
	= grub_cpu_to_be32 (core_size + 0x200 - GRUB_BOOT_AOUT_HEADER_SIZE);
    }
  else
    {
      *((grub_uint32_t *) (boot_img + GRUB_DISK_SECTOR_SIZE
			   - GRUB_BOOT_MACHINE_LIST_SIZE + 8))
	= grub_cpu_to_be32 (num);
    }

  grub_util_write_image (boot_img, boot_size, out);
  free (boot_img);
  free (boot_path);

  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_TOTAL_MODULE_SIZE))
    = grub_cpu_to_be32 (total_module_size);
  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_KERNEL_IMAGE_SIZE))
    = grub_cpu_to_be32 (kernel_size);

  /* No compression support yet.  */
  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_COMPRESSED_SIZE))
    = grub_cpu_to_be32 (0);

  grub_util_write_image (core_img, core_size, out);
  free (kernel_img);
  free (core_img);

  while (path_list)
    {
      struct grub_util_path_list *next = path_list->next;
      free ((void *) path_list->name);
      free (path_list);
      path_list = next;
    }
}

static struct option options[] =
  {
    {"directory", required_argument, 0, 'd'},
    {"prefix", required_argument, 0, 'p'},
    {"memdisk", required_argument, 0, 'm'},
    {"config", required_argument, 0, 'c'},
    {"output", required_argument, 0, 'o'},
    {"netboot", no_argument, 0, 'n'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr, "Try ``grub-mkimage --help'' for more information.\n");
  else
    printf ("\
Usage: grub-mkimage [OPTION]... [MODULES]\n\
\n\
Make a bootable image of GRUB.\n\
\n\
  -d, --directory=DIR     use images and modules under DIR [default=%s]\n\
  -p, --prefix=DIR        set grub_prefix directory [default=%s]\n\
  -m, --memdisk=FILE      embed FILE as a memdisk image\n\
  -c, --config=FILE       embed FILE as boot config\n\
  -o, --output=FILE       output a generated image to FILE [default=stdout]\n\
  -n, --netboot           generate netboot image\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
\n\
Report bugs to <%s>.\n\
", GRUB_LIBDIR, DEFAULT_DIRECTORY, PACKAGE_BUGREPORT);

  exit (status);
}

int
main (int argc, char *argv[])
{
  char *output = NULL;
  char *dir = NULL;
  char *prefix = NULL;
  char *memdisk = NULL;
  char *config = NULL;
  FILE *fp = stdout;
  int netboot = 0;

  progname = "grub-mkimage";
  while (1)
    {
      int c = getopt_long (argc, argv, "d:p:m:c:o:nhVv", options, 0);

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'o':
	    if (output)
	      free (output);
	    output = xstrdup (optarg);
	    break;

	  case 'd':
	    if (dir)
	      free (dir);
	    dir = xstrdup (optarg);
	    break;

	  case 'm':
	    if (memdisk)
	      free (memdisk);
	    memdisk = xstrdup (optarg);

	    if (prefix)
	      free (prefix);
	    prefix = xstrdup ("(memdisk)/boot/grub");
	    break;

	  case 'c':
	    if (config)
	      free (config);

	    config = xstrdup (optarg);
	    break;

	  case 'n':
	    netboot = 1;
	    break;

	  case 'h':
	    usage (0);
	    break;

	  case 'p':
	    if (prefix)
	      free (prefix);
	    prefix = xstrdup (optarg);
	    break;

	  case 'V':
	    printf ("grub-mkimage (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    verbosity++;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  if (output)
    {
      fp = fopen (output, "wb");
      if (! fp)
	grub_util_error ("cannot open %s", output);
    }

  generate_image (dir ? : GRUB_LIBDIR, prefix ? : DEFAULT_DIRECTORY, fp,
		  argv + optind, memdisk, config, netboot);

  fclose (fp);

  if (dir)
    free (dir);

  if (memdisk)
    free (memdisk);

  if (config)
    free (config);

  return 0;
}
