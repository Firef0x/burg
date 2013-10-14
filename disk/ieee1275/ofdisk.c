/* ofdisk.c - Open Firmware disk access.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/mm.h>
#include <grub/ieee1275/ieee1275.h>
#include <grub/ieee1275/ofdisk.h>

struct ofdisk_hash_ent
{
  char *devpath;
  int refs;
  grub_ieee1275_ihandle_t dev_ihandle;
  struct ofdisk_hash_ent *next;
};

#define OFDISK_HASH_SZ	8
static struct ofdisk_hash_ent *ofdisk_hash[OFDISK_HASH_SZ];

static int
ofdisk_hash_fn (const char *devpath)
{
  int hash = 0;
  while (*devpath)
    hash ^= *devpath++;
  return (hash & (OFDISK_HASH_SZ - 1));
}

static struct ofdisk_hash_ent *
ofdisk_hash_find (const char *devpath)
{
  struct ofdisk_hash_ent *p = ofdisk_hash[ofdisk_hash_fn(devpath)];

  while (p)
    {
      if (!grub_strcmp (p->devpath, devpath))
	break;
      p = p->next;
    }
  return p;
}

static struct ofdisk_hash_ent *
ofdisk_hash_add (char *devpath)
{
  struct ofdisk_hash_ent **head = &ofdisk_hash[ofdisk_hash_fn(devpath)];
  struct ofdisk_hash_ent *p = grub_malloc(sizeof (*p));

  if (p)
    {
      p->devpath = grub_strdup (devpath);
      p->next = *head;
      p->refs = 0;
      p->dev_ihandle = 0;
      *head = p;
    }
  return p;
}

struct grub_ofdisk_iterate_closure
{
  int (*hook) (const char *name, void *closure);
  void *closure;
};

static int
dev_iterate (struct grub_ieee1275_devalias *alias, void *closure)
{
  struct grub_ofdisk_iterate_closure *c = closure;
  int ret = 0;

  grub_dprintf ("disk", "disk name = %s\n", alias->name);

  if (grub_ieee1275_test_flag (GRUB_IEEE1275_FLAG_OFDISK_SDCARD_ONLY))
    {
      grub_ieee1275_phandle_t dev;
      char tmp[8];

      if (grub_ieee1275_finddevice (alias->path, &dev))
	{
	  grub_dprintf ("disk", "finddevice (%s) failed\n", alias->path);
	  return 0;
	}

      if (grub_ieee1275_get_property (dev, "iconname", tmp,
				      sizeof tmp, 0))
	{
	  grub_dprintf ("disk", "get iconname failed\n");
	  return 0;
	}

      if (grub_strcmp (tmp, "sdmmc"))
	{
	  grub_dprintf ("disk", "device is not an SD card\n");
	  return 0;
	}
    }

  if (! grub_strcmp (alias->type, "block") &&
      grub_strncmp (alias->name, "cdrom", 5))
    ret = c->hook (alias->name, c->closure);
  return ret;
}

static int
grub_ofdisk_iterate (int (*hook) (const char *name, void *closure), void *closure)
{
  struct grub_ofdisk_iterate_closure c;

  c.hook = hook;
  c.closure = closure;
  return grub_devalias_iterate (dev_iterate, &c);
}

static char *
compute_dev_path (const char *name)
{
  char *devpath = grub_malloc (grub_strlen (name) + 3);
  char *p, c;

  if (!devpath)
    return NULL;

  /* Un-escape commas. */
  p = devpath;
  while ((c = *name++) != '\0')
    {
      if (c == '\\' && *name == ',')
	{
	  *p++ = ',';
	  name++;
	}
      else
	*p++ = c;
    }

  if (! grub_ieee1275_test_flag (GRUB_IEEE1275_FLAG_NO_PARTITION_0))
    {
      *p++ = ':';
      *p++ = '0';
    }
  *p++ = '\0';

  return devpath;
}

static grub_err_t
grub_ofdisk_open (const char *name, grub_disk_t disk)
{
  grub_ieee1275_phandle_t dev;
  grub_ieee1275_ihandle_t dev_ihandle = 0;
  struct ofdisk_hash_ent *op;
  char *devpath;
  /* XXX: This should be large enough for any possible case.  */
  char prop[64];
  grub_ssize_t actual;

  devpath = compute_dev_path (name);
  if (! devpath)
    return grub_errno;

  op = ofdisk_hash_find (devpath);
  if (!op)
    op = ofdisk_hash_add (devpath);

  grub_free (devpath);
  if (!op)
    return grub_errno;

  if (op->dev_ihandle)
    {
      op->refs++;

      /* XXX: There is no property to read the number of blocks.  There
	 should be a property `#blocks', but it is not there.  Perhaps it
	 is possible to use seek for this.  */
      disk->total_sectors = 0xFFFFFFFFUL;

      disk->id = (unsigned long) op;

      /* XXX: Read this, somehow.  */
      disk->has_partitions = 1;
      disk->data = op;
      return 0;
    }

  grub_dprintf ("disk", "Opening `%s'.\n", op->devpath);

  if (grub_ieee1275_finddevice (op->devpath, &dev))
    {
      grub_error (GRUB_ERR_UNKNOWN_DEVICE, "can't read device properties");
      goto fail;
    }

  if (grub_ieee1275_get_property (dev, "device_type", prop, sizeof (prop),
				  &actual))
    {
      grub_error (GRUB_ERR_UNKNOWN_DEVICE, "can't read the device type");
      goto fail;
    }

  if (grub_strcmp (prop, "block"))
    {
      grub_error (GRUB_ERR_BAD_DEVICE, "not a block device");
      goto fail;
    }

  grub_ieee1275_open (op->devpath, &dev_ihandle);
  if (! dev_ihandle)
    {
      grub_error (GRUB_ERR_UNKNOWN_DEVICE, "can't open device");
      goto fail;
    }

  grub_dprintf ("disk", "Opened `%s' as handle %p.\n", op->devpath,
		(void *) (unsigned long) dev_ihandle);

  op->dev_ihandle = dev_ihandle;
  op->refs++;

  /* XXX: There is no property to read the number of blocks.  There
     should be a property `#blocks', but it is not there.  Perhaps it
     is possible to use seek for this.  */
  disk->total_sectors = 0xFFFFFFFFUL;

  disk->id = (unsigned long) op;

  /* XXX: Read this, somehow.  */
  disk->has_partitions = 1;
  disk->data = op;
  return 0;

 fail:
  if (dev_ihandle)
    grub_ieee1275_close (dev_ihandle);
  return grub_errno;
}

static void
grub_ofdisk_close (grub_disk_t disk)
{
  struct ofdisk_hash_ent *data = disk->data;

  data->refs--;
  if (data->refs)
    return;

  grub_dprintf ("disk", "Closing handle %p.\n", data);
  grub_ieee1275_close (data->dev_ihandle);
  data->dev_ihandle = 0;
}

static grub_err_t
grub_ofdisk_read (grub_disk_t disk, grub_disk_addr_t sector,
		  grub_size_t size, char *buf)
{
  grub_ssize_t status, actual;
  unsigned long long pos;
  struct ofdisk_hash_ent *data = disk->data;

  pos = sector * 512UL;

  grub_ieee1275_seek (data->dev_ihandle, pos, &status);
  if (status < 0)
    return grub_error (GRUB_ERR_READ_ERROR,
		       "seek error, can't seek block %llu",
		       (long long) sector);
  size <<= 9;
  grub_ieee1275_read (data->dev_ihandle, buf, size, &actual);
  if (actual != (int) size)
    return grub_error (GRUB_ERR_READ_ERROR, "read error on block: %llu",
		       (long long) sector);

  return 0;
}

static grub_err_t
grub_ofdisk_write (grub_disk_t disk __attribute ((unused)),
		   grub_disk_addr_t sector __attribute ((unused)),
		   grub_size_t size __attribute ((unused)),
		   const char *buf __attribute ((unused)))
{
  return GRUB_ERR_NOT_IMPLEMENTED_YET;
}

static struct grub_disk_dev grub_ofdisk_dev =
  {
    .name = "ofdisk",
    .id = GRUB_DISK_DEVICE_OFDISK_ID,
    .iterate = grub_ofdisk_iterate,
    .open = grub_ofdisk_open,
    .close = grub_ofdisk_close,
    .read = grub_ofdisk_read,
    .write = grub_ofdisk_write,
    .next = 0
  };

void
grub_ofdisk_init (void)
{
  grub_disk_dev_register (&grub_ofdisk_dev);
}

void
grub_ofdisk_fini (void)
{
  grub_disk_dev_unregister (&grub_ofdisk_dev);
}
