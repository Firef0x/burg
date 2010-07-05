/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/machine/biosdisk.h>
#include <grub/machine/memory.h>
#include <grub/machine/kernel.h>
#include <grub/disk.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/term.h>
#include <grub/fbfs.h>

static int cd_drive = -1;

#define DISK_NUM_INC	4

static struct grub_biosdisk_data *grub_biosdisk_geom;
static int grub_biosdisk_num;

static struct grub_biosdisk_data *
get_drive_geom (int drive)
{
  int i;
  struct grub_biosdisk_data *data = grub_biosdisk_geom;
  grub_uint64_t total_sectors = 0;
  unsigned long cylinders;
  int is_fb = 0;
  struct fb_mbr *m;

  for (i = 0; i < grub_biosdisk_num; i++, data++)
    if (data->drive == drive)
      return data;

  if ((drive != cd_drive) && ((drive >= 0x80) || (drive == grub_boot_drive)))
    {
      if (grub_biosdisk_rw_standard (0x02, drive, 0, 0, 1, 1,
				     GRUB_MEMORY_MACHINE_SCRATCH_SEG) != 0)
	return 0;
    }

  m = (struct fb_mbr *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR;
  if ((grub_biosdisk_num & (DISK_NUM_INC - 1)) == 0)
    {
      grub_biosdisk_geom = grub_realloc (grub_biosdisk_geom,
					 (grub_biosdisk_num + DISK_NUM_INC) *
					 sizeof (*data));
      if (! grub_biosdisk_geom)
	return 0;

      is_fb = ((m->fb_magic == FB_MAGIC_LONG) && (m->end_magic == 0xaa55));
    }

  data = &grub_biosdisk_geom[grub_biosdisk_num++];
  grub_memset (data, 0, sizeof (*data));
  data->drive = drive;

  if (drive == cd_drive)
    {
      data->flags = GRUB_BIOSDISK_FLAG_LBA | GRUB_BIOSDISK_FLAG_CDROM;
      data->sectors = 32;
      data->total_sectors = GRUB_ULONG_MAX;  /* TODO: get the correct size.  */
      return data;
    }

  if (drive & 0x80)
    {
      /* HDD */
      int version;

      version = grub_biosdisk_check_int13_extensions (drive);
      if (version)
	{
	  struct grub_biosdisk_drp *drp
	    = (struct grub_biosdisk_drp *)
	    (GRUB_MEMORY_MACHINE_SCRATCH_ADDR + 512);

	  /* Clear out the DRP.  */
	  grub_memset (drp, 0, sizeof (*drp));
	  drp->size = sizeof (*drp);
	  if (! grub_biosdisk_get_diskinfo_int13_extensions (drive, drp))
	    {
	      data->flags = GRUB_BIOSDISK_FLAG_LBA;

	      if (drp->cylinders == 65535)
		total_sectors = GRUB_ULONG_MAX;
	      else if (drp->total_sectors)
		total_sectors = drp->total_sectors;
	      else
                /* Some buggy BIOSes doesn't return the total sectors
                   correctly but returns zero. So if it is zero, compute
                   it by C/H/S returned by the LBA BIOS call.  */
                total_sectors = drp->cylinders * drp->heads * drp->sectors;
	    }
	}
    }

  data->sectors = 63;
  data->heads = 255;
  cylinders = 0;
  grub_biosdisk_get_diskinfo_standard (drive, &cylinders,
				       &data->heads, &data->sectors);
  if (is_fb)
    {
      grub_uint16_t ofs = m->lba;

      data->max_sectors = m->max_sec;
      if (data->max_sectors >= 0x80)
	{
	  data->max_sectors &= 0x7f;
	  data->flags &= ~GRUB_BIOSDISK_FLAG_LBA;
	}

      if (! (data->flags & GRUB_BIOSDISK_FLAG_LBA))
	{
	  grub_uint16_t lba;

	  if (grub_biosdisk_rw_standard (0x02, drive,
					 0, 1, 1, 1,
					 GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	    goto next;

	  lba = m->end_magic;
	  if (lba == 0xaa55)
	    {
	      if (m->fb_magic != FB_MAGIC_LONG)
		goto next;
	      else
		lba = m->lba;
	    }

	  data->sectors = lba - ofs;

	  if (grub_biosdisk_rw_standard (0x02, drive,
					 1, 0, 1, 1,
					 GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	    goto next;

	  lba = m->end_magic;
	  if (lba == 0xaa55)
	    goto next;

	  data->heads = (lba - ofs) / data->sectors;
	}

      data->flags |= GRUB_BIOSDISK_FLAG_FB;
    }

 next:
  cylinders *= data->heads * data->sectors;
  if (total_sectors < cylinders)
    total_sectors = cylinders;

  data->total_sectors = total_sectors;
  return data;
}

static int
grub_biosdisk_get_drive (const char *name)
{
  unsigned long drive;

  if ((name[0] != 'f' && name[0] != 'h') || name[1] != 'd')
    goto fail;

  drive = grub_strtoul (name + 2, 0, 10);
  if (grub_errno != GRUB_ERR_NONE)
    goto fail;

  if (name[0] == 'h')
    drive += 0x80;

  return (int) drive ;

 fail:
  grub_error (GRUB_ERR_UNKNOWN_DEVICE, "not a biosdisk");
  return -1;
}

static int
grub_biosdisk_call_hook (int (*hook) (const char *name, void *closure),
			 void *closure, int drive)
{
  char name[10];
  struct grub_biosdisk_data *data;

  data = get_drive_geom (drive);
  if (! data)
    return 0;

  grub_snprintf (name, sizeof (name),
		 (drive & 0x80) ? "hd%d" : "fd%d", drive & (~0x80));
  if (hook (name, closure))
    return 1;

  if ((data->flags & GRUB_BIOSDISK_FLAG_FB) && (drive == grub_boot_drive))
    {
      if (hook ("fb", closure))
	return 1;
    }

  return 0;
}

static int
grub_biosdisk_iterate (int (*hook) (const char *name, void *closure),
		       void *closure)
{
  int drive;
  int num_floppies;

  /* For hard disks, attempt to read the MBR.  */
  for (drive = 0x80; drive < 0x90; drive++)
    {
      if (grub_biosdisk_call_hook (hook, closure, drive))
	return 1;
    }

  if (cd_drive >= 0x90)
    {
      if (grub_biosdisk_call_hook (hook, closure, cd_drive))
      return 1;
    }

  /* For floppy disks, we can get the number safely.  */
  num_floppies = grub_biosdisk_get_num_floppies ();
  for (drive = 0; drive < num_floppies; drive++)
    if (grub_biosdisk_call_hook (hook, closure, drive))
      return 1;

  return 0;
}

static grub_err_t
grub_biosdisk_open (const char *name, grub_disk_t disk)
{
  int drive;
  struct grub_biosdisk_data *data;

  drive = (! grub_strcmp (name, "fb")) ? grub_boot_drive :
    grub_biosdisk_get_drive (name);
  if (drive < 0)
    return grub_errno;

  data = get_drive_geom (drive);
  if (! data)
    return grub_error (GRUB_ERR_BAD_DEVICE, "invalid drive");

  disk->has_partitions = (drive != cd_drive);
  disk->id = drive;
  disk->total_sectors = data->total_sectors;
  disk->data = data;

  return GRUB_ERR_NONE;
}

static void
grub_biosdisk_close (grub_disk_t disk __attribute ((unused)))
{
}

/* For readability.  */
#define GRUB_BIOSDISK_READ	0
#define GRUB_BIOSDISK_WRITE	1

#define GRUB_BIOSDISK_CDROM_RETRY_COUNT 3

static grub_err_t
grub_biosdisk_rw (int cmd, grub_disk_t disk,
		  grub_disk_addr_t sector, grub_size_t size,
		  unsigned segment)
{
  struct grub_biosdisk_data *data = disk->data;

  if (data->flags & GRUB_BIOSDISK_FLAG_LBA)
    {
      struct grub_biosdisk_dap *dap;

      dap = (struct grub_biosdisk_dap *) (GRUB_MEMORY_MACHINE_SCRATCH_ADDR
					  + (data->sectors
					     << GRUB_DISK_SECTOR_BITS));
      dap->length = sizeof (*dap);
      dap->reserved = 0;
      dap->blocks = size;
      dap->buffer = segment << 16;	/* The format SEGMENT:ADDRESS.  */
      dap->block = sector;

      if (data->flags & GRUB_BIOSDISK_FLAG_CDROM)
        {
	  int i;

	  if (cmd)
	    return grub_error (GRUB_ERR_WRITE_ERROR, "can\'t write to cdrom");

	  dap->blocks = ALIGN_UP (dap->blocks, 4) >> 2;
	  dap->block >>= 2;

	  for (i = 0; i < GRUB_BIOSDISK_CDROM_RETRY_COUNT; i++)
            if (! grub_biosdisk_rw_int13_extensions (0x42, data->drive, dap))
	      break;

	  if (i == GRUB_BIOSDISK_CDROM_RETRY_COUNT)
	    return grub_error (GRUB_ERR_READ_ERROR, "cdrom read error");
	}
      else
        if (grub_biosdisk_rw_int13_extensions (cmd + 0x42, data->drive, dap))
	  {
	    /* Fall back to the CHS mode.  */
	    data->flags &= ~GRUB_BIOSDISK_FLAG_LBA;
	    return grub_biosdisk_rw (cmd, disk, sector, size, segment);
	  }
    }
  else
    {
      unsigned coff, hoff, soff;
      unsigned head;

      /* It is impossible to reach over 8064 MiB (a bit less than LBA24) with
	 the traditional CHS access.  */
      if (sector >
	  1024 /* cylinders */ *
	  256 /* heads */ *
	  63 /* spt */)
	return grub_error (GRUB_ERR_OUT_OF_RANGE, "%s out of disk", disk->name);

      soff = ((grub_uint32_t) sector) % data->sectors + 1;
      head = ((grub_uint32_t) sector) / data->sectors;
      hoff = head % data->heads;
      coff = head / data->heads;

      if (grub_biosdisk_rw_standard (cmd + 0x02, data->drive,
				     coff, hoff, soff, size, segment))
	{
	  switch (cmd)
	    {
	    case GRUB_BIOSDISK_READ:
	      return grub_error (GRUB_ERR_READ_ERROR, "%s read error", disk->name);
	    case GRUB_BIOSDISK_WRITE:
	      return grub_error (GRUB_ERR_WRITE_ERROR, "%s write error", disk->name);
	    }
	}
    }

  return GRUB_ERR_NONE;
}

/* Return the number of sectors which can be read safely at a time.  */
static grub_size_t
get_safe_sectors (grub_disk_addr_t sector, grub_uint32_t sectors)
{
  grub_size_t size;
  grub_uint32_t offset;

  /* OFFSET = SECTOR % SECTORS */
  grub_divmod64 (sector, sectors, &offset);

  size = sectors - offset;

  /* Limit the max to 0x7f because of Phoenix EDD.  */
  if (size > 0x7f)
    size = 0x7f;

  return size;
}

static grub_err_t
grub_biosdisk_read (grub_disk_t disk, grub_disk_addr_t sector,
		    grub_size_t size, char *buf)
{
  struct grub_biosdisk_data *data = disk->data;

  while (size)
    {
      grub_size_t len;
      grub_size_t cdoff = 0;

      len = get_safe_sectors (sector, data->sectors);
      if ((data->max_sectors) && ((int) len > data->max_sectors))
	len = data->max_sectors;

      if (data->flags & GRUB_BIOSDISK_FLAG_CDROM)
	{
	  cdoff = (sector & 3) << GRUB_DISK_SECTOR_BITS;
	  len = ALIGN_UP (sector + len, 4) - (sector & ~3);
	  sector &= ~3;
	}

      if (len > size)
	len = size;

      if (grub_biosdisk_rw (GRUB_BIOSDISK_READ, disk, sector, len,
			    GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	return grub_errno;

      grub_memcpy (buf, (void *) (GRUB_MEMORY_MACHINE_SCRATCH_ADDR + cdoff),
		   len << GRUB_DISK_SECTOR_BITS);
      buf += len << GRUB_DISK_SECTOR_BITS;
      sector += len;
      size -= len;
    }

  return grub_errno;
}

static grub_err_t
grub_biosdisk_write (grub_disk_t disk, grub_disk_addr_t sector,
		     grub_size_t size, const char *buf)
{
  struct grub_biosdisk_data *data = disk->data;

  if (data->flags & GRUB_BIOSDISK_FLAG_CDROM)
    return grub_error (GRUB_ERR_IO, "can't write to CDROM");

  while (size)
    {
      grub_size_t len;

      len = get_safe_sectors (sector, data->sectors);
      if (len > size)
	len = size;

      grub_memcpy ((void *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR, buf,
		   len << GRUB_DISK_SECTOR_BITS);

      if (grub_biosdisk_rw (GRUB_BIOSDISK_WRITE, disk, sector, len,
			    GRUB_MEMORY_MACHINE_SCRATCH_SEG))
	return grub_errno;

      buf += len << GRUB_DISK_SECTOR_BITS;
      sector += len;
      size -= len;
    }

  return grub_errno;
}

static struct grub_disk_dev grub_biosdisk_dev =
  {
    .name = "biosdisk",
    .id = GRUB_DISK_DEVICE_BIOSDISK_ID,
    .iterate = grub_biosdisk_iterate,
    .open = grub_biosdisk_open,
    .close = grub_biosdisk_close,
    .read = grub_biosdisk_read,
    .write = grub_biosdisk_write,
    .next = 0
  };

static void
grub_disk_biosdisk_fini (void)
{
  grub_disk_dev_unregister (&grub_biosdisk_dev);
  grub_free (grub_biosdisk_geom);
}

GRUB_MOD_INIT(biosdisk)
{
  struct grub_biosdisk_cdrp *cdrp
        = (struct grub_biosdisk_cdrp *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR;

  if (grub_disk_firmware_is_tainted)
    {
      grub_printf ("Firmware is marked as tainted, refusing to initialize.\n");
      return;
    }
  grub_disk_firmware_fini = grub_disk_biosdisk_fini;

  grub_memset (cdrp, 0, sizeof (*cdrp));
  cdrp->size = sizeof (*cdrp);
  cdrp->media_type = 0xFF;
  if ((! grub_biosdisk_get_cdinfo_int13_extensions (grub_boot_drive, cdrp)) &&
      ((cdrp->media_type & GRUB_BIOSDISK_CDTYPE_MASK)
       == GRUB_BIOSDISK_CDTYPE_NO_EMUL))
    cd_drive = cdrp->drive_no;

  grub_disk_dev_register (&grub_biosdisk_dev);
}

GRUB_MOD_FINI(biosdisk)
{
  grub_disk_biosdisk_fini ();
}
