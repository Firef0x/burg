/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
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
#include <grub/fs.h>
#include <grub/mm.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/machine/boot.h>
#include <grub/machine/kernel.h>
#include <grub/machine/memory.h>
#include <grub/machine/biosdisk.h>

#define FB_MAGIC	"FBBF"
#define FB_MAGIC_LONG	0x46424246

#define FBM_TYPE_FILE	1

struct fb_mbr
{
  grub_uint8_t jmp_code;
  grub_uint8_t jmp_ofs;
  grub_uint8_t boot_code[0x1a9];
  grub_uint8_t max_sec;		/* 0x1ab  */
  grub_uint16_t lba;		/* 0x1ac  */
  grub_uint8_t spt;		/* 0x1ae  */
  grub_uint8_t heads;		/* 0x1af  */
  grub_uint16_t boot_base;	/* 0x1b0  */
  grub_uint16_t boot_size;	/* 0x1b2  */
  grub_uint32_t fb_magic;	/* 0x1b4  */
  grub_uint8_t mbr_table[0x46];	/* 0x1b8  */
  grub_uint16_t end_magic;	/* 0x1fe  */
} __attribute__((packed));

struct fb_data
{
  grub_uint16_t menu_ofs;	/* 0x200  */
  grub_uint16_t flags;		/* 0x202  */
  grub_uint8_t ver_major;	/* 0x204  */
  grub_uint8_t ver_minor;	/* 0x205  */
  grub_uint32_t pri_size;	/* 0x206  */
  grub_uint32_t ext_size;	/* 0x20a  */
} __attribute__((packed));

struct fbm_file
{
  grub_uint8_t size;
  grub_uint8_t type;
  grub_uint32_t data_start;
  grub_uint32_t data_size;
  grub_uint32_t data_time;
  grub_uint8_t flag;
  char name[0];
} __attribute__((packed));

static int fb_drive, fb_lba, fb_nhd, fb_spt, fb_max;
static char *fb_list;
static grub_uint32_t fb_ofs, fb_pri_size, fb_total_size;

static int
fb_rw_lba (int cmd, grub_disk_addr_t sector, int size)
{
  struct grub_biosdisk_dap *dap;
  dap = (struct grub_biosdisk_dap *) (GRUB_MEMORY_MACHINE_SCRATCH_ADDR
				      + (63 << GRUB_DISK_SECTOR_BITS));

  dap->length = sizeof (*dap);
  dap->reserved = 0;
  dap->blocks = size;
  dap->buffer = GRUB_MEMORY_MACHINE_SCRATCH_SEG << 16;
  dap->block = sector;
  return grub_biosdisk_rw_int13_extensions (cmd + 0x42, fb_drive, dap);
}

static int
fb_rw_chs (int cmd, grub_uint32_t sector, int size)
{
  unsigned coff, hoff, soff, head;

  soff = sector % fb_spt + 1;
  head = sector / fb_spt;
  hoff = head % fb_nhd;
  coff = head / fb_nhd;

  return grub_biosdisk_rw_standard (cmd + 0x02, fb_drive,
				    coff, hoff, soff, size,
				    GRUB_MEMORY_MACHINE_SCRATCH_SEG);
}

static int
fb_rw (int cmd, char *buf, grub_disk_addr_t sector, int ofs, int size,
       void (*hook) (grub_disk_addr_t sector,
		     unsigned offset, unsigned length, void* closure),
       void *closure)
{
  sector -= fb_ofs;
  while (size)
    {
      int ns, nb;

      nb = size;
      ns = (ofs + size + 511) >> 9;
      if (ns > fb_max)
	{
	  ns = fb_max;
	  nb = (ns << 9) - ofs;
	}

      if (cmd)
	grub_memcpy ((char *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR + ofs, buf, nb);

      if ((fb_lba) ? fb_rw_lba (cmd, sector, ns) : fb_rw_chs (cmd, sector, ns))
	return 1;

      if (buf)
	{
	  if (! cmd)
	    grub_memcpy (buf, (char *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR + ofs,
			 nb);
	  buf += nb;
	}

      if (hook)
	{
	  while (nb)
	    {
	      int n;

	      n = nb;
	      if (ofs + nb > GRUB_DISK_SECTOR_SIZE)
		n = GRUB_DISK_SECTOR_SIZE - ofs;
	      hook (sector, ofs, n, closure);
	      sector++;
	      nb -= n;
	      size -= n;
	      ofs = 0;
	    }
	}
      else
	{
	  sector += ns;
	  size -= nb;
	  ofs = 0;
	}
    }

  return 0;
}

static int
fb_detect (void)
{
  struct fb_mbr *m;
  struct fb_data *data;
  int boot_base, boot_size, i, menu_ofs, list_size;
  char *p;

  fb_drive = grub_boot_drive;

  if (fb_drive == GRUB_BOOT_MACHINE_PXE_DL)
    return 0;

  if (! fb_rw_lba (0, 0, 1))
    fb_lba = 1;
  else if (grub_biosdisk_rw_standard (0x02, fb_drive,
				      0, 0, 1, 1,
				      GRUB_MEMORY_MACHINE_SCRATCH_SEG))
    return 0;

  m =  (struct fb_mbr *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR;
  if ((m->fb_magic != FB_MAGIC_LONG) || (m->end_magic != 0xaa55))
    return 0;

  fb_ofs = m->lba;
  fb_max = m->max_sec;
  boot_base = m->boot_base;
  boot_size = m->boot_size;

  if (! fb_lba)
    {
      grub_uint16_t lba;

      if (grub_biosdisk_rw_standard (0x02, fb_drive,
				      0, 1, 1, 1,
				      GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	return 0;

      lba = m->end_magic;
      if (lba == 0xaa55)
	{
	  if (m->fb_magic != FB_MAGIC_LONG)
	    return 0;
	  else
	    lba = m->lba;
	}
      fb_spt = lba - fb_ofs;

      if (grub_biosdisk_rw_standard (0x02, fb_drive,
				     1, 0, 1, 1,
				     GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	return 0;

      lba = m->end_magic;
      if (lba == 0xaa55)
	return 0;
      fb_nhd = (lba - fb_ofs) / fb_spt;
    }

  fb_list = grub_malloc (boot_size << 9);
  if (! fb_list)
    return 0;

  if (fb_rw (0, fb_list, boot_base + 1, 0, boot_size << 9, 0, 0))
    return 0;

  data = (struct fb_data *) fb_list;
  if ((data->ver_major != 1) || (data->ver_minor < 5))
    return 0;

  menu_ofs = data->menu_ofs;
  fb_pri_size = data->pri_size;
  fb_total_size = data->pri_size + data->ext_size;

  i = (menu_ofs >> 9) + 1;
  p = fb_list + (i << 9) - 2;
  for (; i < boot_size; i++)
    {
      grub_memcpy (p, fb_list + (i << 9), 510);
      p += 510;
    }

  list_size = p - fb_list;

  p = fb_list;
  while (fb_list[menu_ofs])
    {
      int len;

      len = fb_list[menu_ofs] + 2;

      if (fb_list[menu_ofs + 1] == FBM_TYPE_FILE)
	{
	  grub_memcpy (p, fb_list + menu_ofs, len);
	  p += len;
	}

      menu_ofs += len;
      if (menu_ofs >= list_size)
	return 0;
    }
  *p = 0;

  return 1;
}

static int
grub_fb_iterate (int (*hook) (const char *name, void *closure),
		 void *closure)
{
  if (hook ("fb", closure))
    return 1;
  return 0;
}

static grub_err_t
grub_fb_open (const char *name, grub_disk_t disk)
{
  if (grub_strcmp (name, "fb") != 0)
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "not a fb disk");

  disk->has_partitions = 0;
  disk->total_sectors = fb_total_size;

  return GRUB_ERR_NONE;
}

static void
grub_fb_close (grub_disk_t disk __attribute ((unused)))
{
}

static grub_err_t
grub_fb_read (grub_disk_t disk, grub_disk_addr_t sector,
	      grub_size_t size, char *buf)
{
  return (fb_rw (0, buf, sector, 0, size << 9,
		 disk->read_hook, disk->closure)) ?
    grub_error (GRUB_ERR_READ_ERROR, "read error") : GRUB_ERR_NONE;
}

static grub_err_t
grub_fb_write (grub_disk_t disk, grub_disk_addr_t sector,
	       grub_size_t size, const char *buf)
{
  return (fb_rw (1, (char *) buf, sector, 0, size << 9,
		 disk->read_hook, disk->closure)) ?
    grub_error (GRUB_ERR_WRITE_ERROR, "write error") : GRUB_ERR_NONE;
}

static struct grub_disk_dev grub_fb_dev =
  {
    .name = "fb",
    .id = GRUB_DISK_DEVICE_FB_ID,
    .iterate = grub_fb_iterate,
    .open = grub_fb_open,
    .close = grub_fb_close,
    .read = grub_fb_read,
    .write = grub_fb_write,
    .next = 0
  };

static grub_err_t
grub_fbfs_dir (grub_device_t device, const char *path,
	       int (*hook) (const char *filename,
			    const struct grub_dirhook_info *info,
			    void *closure),
	       void *closure)
{
  struct grub_dirhook_info info;
  struct fbm_file *p;
  char *fn;
  int len, ofs;

  if (device->disk->dev->id != GRUB_DISK_DEVICE_FB_ID)
    return grub_error (GRUB_ERR_BAD_FS, "not a fb disk");

  while (*path == '/')
    path++;
  len = grub_strlen (path);
  fn = grub_strrchr (path, '/');
  ofs = (fn) ? (fn + 1 - path) : 0;

  grub_memset (&info, 0, sizeof (info));
  p = (struct fbm_file *) fb_list;
  while (p->size)
    {
      if ((! grub_memcmp (path, p->name, len)) &&
	  (hook (p->name + ofs, &info, closure)))
	break;

      p = (struct fbm_file *) ((char *) p + p->size + 2);
    }

  return GRUB_ERR_NONE;
}


static grub_err_t
grub_fbfs_open (struct grub_file *file, const char *name)
{
  struct fbm_file *p;

  if (file->device->disk->dev->id != GRUB_DISK_DEVICE_FB_ID)
    return grub_error (GRUB_ERR_BAD_FS, "not a fb disk");

  while (*name == '/')
    name++;

  p = (struct fbm_file *) fb_list;
  while (p->size)
    {
      if (! grub_strcmp (name, p->name))
	{
	  file->data = p;
	  file->size = p->data_size;
	  return GRUB_ERR_NONE;
	}

      p = (struct fbm_file *) ((char *) p + p->size + 2);
    }

  return grub_error (GRUB_ERR_FILE_NOT_FOUND, "file not found");
}

static grub_ssize_t
grub_fbfs_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct fbm_file *p;
  grub_uint32_t sector;
  grub_size_t saved_len, ofs;

  p = file->data;
  if (p->data_start >= fb_pri_size)
    return (fb_rw (0, buf, p->data_start + (file->offset >> 9),
		   file->offset & 0x1ff, len,
		   file->read_hook, file->closure)) ? -1 : (grub_ssize_t) len;

  sector = p->data_start + ((grub_uint32_t) file->offset / 510);
  ofs = ((grub_uint32_t) file->offset % 510);
  saved_len = len;
  while (len)
    {
      int n;

      n = len;
      if (ofs + n > 510)
	n = 510 - ofs;
      if (fb_rw (0, buf, sector, ofs, n, file->read_hook, file->closure))
	return -1;
      sector++;
      buf += n;
      len -= n;
      ofs = 0;
    }

  return saved_len;
}

static grub_err_t
grub_fbfs_close (grub_file_t file __attribute ((unused)))
{
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_fbfs_label (grub_device_t device __attribute ((unused)),
		 char **label __attribute ((unused)))
{
  *label = 0;
  return GRUB_ERR_NONE;
}

static struct grub_fs grub_fb_fs =
  {
    .name = "fbfs",
    .dir = grub_fbfs_dir,
    .open = grub_fbfs_open,
    .read = grub_fbfs_read,
    .close = grub_fbfs_close,
    .label = grub_fbfs_label,
    .next = 0
  };

GRUB_MOD_INIT(fb)
{
  if (fb_detect ())
    {
      grub_disk_dev_register (&grub_fb_dev);
      grub_fs_register (&grub_fb_fs);
    }
  else
    {
      fb_drive = -1;
      grub_free (fb_list);
    }
}

GRUB_MOD_FINI(fb)
{
  if (fb_drive >= 0)
    {
      grub_free (fb_list);
      grub_disk_dev_unregister (&grub_fb_dev);
      grub_fs_unregister (&grub_fb_fs);
    }
}
