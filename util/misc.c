/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <grub/kernel.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/cache.h>
#include <grub/util/misc.h>
#include <grub/mm.h>
#include <grub/term.h>
#include <grub/time.h>
#include <grub/i18n.h>

#define ENABLE_RELOCATABLE 0
#include "progname.h"

/* Include malloc.h, only if memalign is available. It is known that
   memalign is declared in malloc.h in all systems, if present.  */
#ifdef HAVE_MEMALIGN
# include <malloc.h>
#endif

#ifdef __CYGWIN__
# include <sys/cygwin.h>
# define DEV_CYGDRIVE_MAJOR 98
#endif

#ifdef __MINGW32__
#include <windows.h>
#include <winioctl.h>
#endif

int verbosity = 0;

void
grub_util_warn (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, _("%s: warn:"), program_name);
  fprintf (stderr, " ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ".\n");
  fflush (stderr);
}

void
grub_util_info (const char *fmt, ...)
{
  if (verbosity > 0)
    {
      va_list ap;

      fprintf (stderr, _("%s: info:"), program_name);
      fprintf (stderr, " ");
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
      fprintf (stderr, ".\n");
      fflush (stderr);
    }
}

void
grub_util_error (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, _("%s: error:"), program_name);
  fprintf (stderr, " ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ".\n");
  exit (1);
}

#ifdef GRUB_UTIL
int
grub_err_printf (const char *fmt, ...)
{
  va_list ap;
  int ret;

  va_start (ap, fmt);
  ret = vfprintf (stderr, fmt, ap);
  va_end (ap);

  return ret;
}
#endif

void *
xmalloc (size_t size)
{
  void *p;

  p = malloc (size);
  if (! p)
    grub_util_error ("out of memory");

  return p;
}

void *
xrealloc (void *ptr, size_t size)
{
  ptr = realloc (ptr, size);
  if (! ptr)
    grub_util_error ("out of memory");

  return ptr;
}

char *
xstrdup (const char *str)
{
  size_t len;
  char *newstr;

  len = strlen (str);
  newstr = (char *) xmalloc (len + 1);
  memcpy (newstr, str, len + 1);

  return newstr;
}

void *
xmalloc_zero (size_t size)
{
  void *p;

  p = xmalloc (size);
  memset (p, 0, size);

  return p;
}

void *
grub_list_reverse (grub_list_t head)
{
  grub_list_t prev;

  prev = 0;
  while (head)
    {
      grub_list_t temp;

      temp = head->next;
      head->next = prev;
      prev = head;
      head = temp;
    }

  return prev;
}

char *
grub_util_get_path (const char *dir, const char *file)
{
  char *path;

  path = (char *) xmalloc (strlen (dir) + 1 + strlen (file) + 1);
  sprintf (path, "%s/%s", dir, file);
  return path;
}

size_t
grub_util_get_fp_size (FILE *fp)
{
  struct stat st;

  if (fflush (fp) == EOF)
    grub_util_error ("fflush failed");

  if (fstat (fileno (fp), &st) == -1)
    grub_util_error ("fstat failed");

  return st.st_size;
}

size_t
grub_util_get_image_size (const char *path)
{
  struct stat st;

  grub_util_info ("getting the size of %s", path);

  if (stat (path, &st) == -1)
    grub_util_error ("cannot stat %s", path);

  return st.st_size;
}

void
grub_util_read_at (void *img, size_t size, off_t offset, FILE *fp)
{
  if (fseeko (fp, offset, SEEK_SET) == -1)
    grub_util_error ("seek failed");

  if (fread (img, 1, size, fp) != size)
    grub_util_error ("read failed");
}

char *
grub_util_read_image (const char *path)
{
  char *img;
  FILE *fp;
  size_t size;

  grub_util_info ("reading %s", path);

  size = grub_util_get_image_size (path);
  img = (char *) xmalloc (size);

  fp = fopen (path, "rb");
  if (! fp)
    grub_util_error ("cannot open %s", path);

  grub_util_read_at (img, size, 0, fp);

  fclose (fp);

  return img;
}

void
grub_util_load_image (const char *path, char *buf)
{
  FILE *fp;
  size_t size;

  grub_util_info ("reading %s", path);

  size = grub_util_get_image_size (path);

  fp = fopen (path, "rb");
  if (! fp)
    grub_util_error ("cannot open %s", path);

  if (fread (buf, 1, size, fp) != size)
    grub_util_error ("cannot read %s", path);

  fclose (fp);
}

void
grub_util_write_image_at (const void *img, size_t size, off_t offset, FILE *out)
{
  grub_util_info ("writing 0x%x bytes at offset 0x%x", size, offset);
  if (fseeko (out, offset, SEEK_SET) == -1)
    grub_util_error ("seek failed");
  if (fwrite (img, 1, size, out) != size)
    grub_util_error ("write failed");
}

void
grub_util_write_image (const char *img, size_t size, FILE *out)
{
  grub_util_info ("writing 0x%x bytes", size);
  if (fwrite (img, 1, size, out) != size)
    grub_util_error ("write failed");
}

char *
grub_util_get_module_name (const char *str)
{
  char *base;
  char *ext;

  base = strrchr (str, '/');
  if (! base)
    base = (char *) str;
  else
    base++;

  ext = strrchr (base, '.');
  if (ext && strcmp (ext, ".mod") == 0)
    {
      char *name;

      name = xmalloc (ext - base + 1);
      memcpy (name, base, ext - base);
      name[ext - base] = '\0';
      return name;
    }

  return xstrdup (base);
}

char *
grub_util_get_module_path (const char *prefix, const char *str)
{
  char *dir;
  char *base;
  char *ext;
  char *ret;

  ext = strrchr (str, '.');
  if (ext && strcmp (ext, ".mod") == 0)
    base = xstrdup (str);
  else
    {
      base = xmalloc (strlen (str) + 4 + 1);
      sprintf (base, "%s.mod", str);
    }

  dir = strchr (str, '/');
  if (dir)
    return base;

  ret = grub_util_get_path (prefix, base);
  free (base);
  return ret;
}

/* Some functions that we don't use.  */
void
grub_mm_init_region (void *addr __attribute__ ((unused)),
		     grub_size_t size __attribute__ ((unused)))
{
}

#if GRUB_NO_MODULES
void
grub_register_exported_symbols (void)
{
}
#endif

void
grub_exit (void)
{
  exit (1);
}

grub_uint32_t
grub_get_rtc (void)
{
  struct timeval tv;

  gettimeofday (&tv, 0);

  return (tv.tv_sec * GRUB_TICKS_PER_SECOND
	  + (((tv.tv_sec % GRUB_TICKS_PER_SECOND) * 1000000 + tv.tv_usec)
	     * GRUB_TICKS_PER_SECOND / 1000000));
}

grub_uint64_t
grub_get_time_ms (void)
{
  struct timeval tv;

  gettimeofday (&tv, 0);

  return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

#ifdef __MINGW32__

void
grub_millisleep (grub_uint32_t ms)
{
  Sleep (ms);
}

#else

void
grub_millisleep (grub_uint32_t ms)
{
  struct timespec ts;

  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep (&ts, NULL);
}

#endif

#if !(defined (__i386__) || defined (__x86_64__)) && GRUB_NO_MODULES
void
grub_arch_sync_caches (void *address __attribute__ ((unused)),
		       grub_size_t len __attribute__ ((unused)))
{
}
#endif

#ifndef HAVE_VASPRINTF

int
vasprintf (char **buf, const char *fmt, va_list ap)
{
  /* Should be large enough.  */
  *buf = xmalloc (512);

  return vsprintf (*buf, fmt, ap);
}

#endif

#ifndef  HAVE_ASPRINTF

int
asprintf (char **buf, const char *fmt, ...)
{
  int status;
  va_list ap;

  va_start (ap, fmt);
  status = vasprintf (buf, fmt, ap);
  va_end (ap);

  return status;
}

#endif

char *
xasprintf (const char *fmt, ...)
{
  va_list ap;
  char *result;

  va_start (ap, fmt);
  if (vasprintf (&result, fmt, ap) < 0)
    {
      if (errno == ENOMEM)
	grub_util_error ("out of memory");
      return NULL;
    }

  return result;
}

#ifdef __MINGW32__

void sync (void)
{
}

int fsync (int fno __attribute__ ((unused)))
{
  return 0;
}

void sleep (int s)
{
  Sleep (s * 1000);
}

grub_int64_t
grub_util_get_disk_size (const char *name)
{
  HANDLE hd;
  grub_int64_t size = -1LL;

  hd = CreateFile (name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   0, OPEN_EXISTING, 0, 0);

  if (hd == INVALID_HANDLE_VALUE)
    return size;

  if (((name[0] == '/') || (name[0] == '\\')) &&
      ((name[1] == '/') || (name[1] == '\\')) &&
      (name[2] == '.') &&
      ((name[3] == '/') || (name[3] == '\\')) &&
      (! strncasecmp (name + 4, "PHYSICALDRIVE", 13)))
    {
      DWORD nr;
      DISK_GEOMETRY g;

      if (! DeviceIoControl (hd, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             0, 0, &g, sizeof (g), &nr, 0))
        goto fail;

      size = g.Cylinders.QuadPart;
      size *= g.TracksPerCylinder * g.SectorsPerTrack * g.BytesPerSector;
    }
  else
    {
      LARGE_INTEGER s;

      s.LowPart = GetFileSize (hd, &s.HighPart);
      size = s.QuadPart;
    }

fail:

  CloseHandle (hd);

  return size;
}

#endif /* __MINGW32__ */

char *
canonicalize_file_name (const char *path)
{
  char *ret;
#ifdef PATH_MAX
  ret = xmalloc (PATH_MAX);
  ret = realpath (path, ret);
#else
  ret = realpath (path, NULL);
#endif
  return ret;
}

#ifdef __CYGWIN__
/* Convert POSIX path to Win32 path,
   remove drive letter, replace backslashes.  */
static char *
get_win32_path (const char *path)
{
  char winpath[PATH_MAX];
  if (cygwin_conv_path (CCP_POSIX_TO_WIN_A, path, winpath, sizeof(winpath)))
    grub_util_error ("cygwin_conv_path() failed");

  int len = strlen (winpath);
  int offs = (len > 2 && winpath[1] == ':' ? 2 : 0);

  int i;
  for (i = offs; i < len; i++)
    if (winpath[i] == '\\')
      winpath[i] = '/';
  return xstrdup (winpath + offs);
}
#endif

/* This function never prints trailing slashes (so that its output
   can be appended a slash unconditionally).  */
char *
make_system_path_relative_to_its_root (const char *path)
{
  struct stat st;
  char *p, *buf, *buf2, *buf3;
  uintptr_t offset = 0;
  dev_t num;
  size_t len;

  /* canonicalize.  */
  p = canonicalize_file_name (path);

  if (p == NULL)
    grub_util_error ("failed to get canonical path of %s", path);

  len = strlen (p) + 1;
  buf = xstrdup (p);
  free (p);

  if (stat (buf, &st) < 0)
    grub_util_error ("cannot stat %s: %s", buf, strerror (errno));

  buf2 = xstrdup (buf);
  num = st.st_dev;

  /* This loop sets offset to the number of chars of the root
     directory we're inspecting.  */
  while (1)
    {
      p = strrchr (buf, '/');
      if (p == NULL)
	/* This should never happen.  */
	grub_util_error ("FIXME: no / in buf. (make_system_path_relative_to_its_root)");
      if (p != buf)
	*p = 0;
      else
	*++p = 0;

      if (stat (buf, &st) < 0)
	grub_util_error ("cannot stat %s: %s", buf, strerror (errno));

      /* buf is another filesystem; we found it.  */
      if (st.st_dev != num)
	{
	  /* offset == 0 means path given is the mount point.
	     This works around special-casing of "/" in Un*x.  This function never
	     prints trailing slashes (so that its output can be appended a slash
	     unconditionally).  Each slash in is considered a preceding slash, and
	     therefore the root directory is an empty string.  */
	  if (offset == 0)
	    {
	      free (buf);
	      free (buf2);
	      return xstrdup ("");
	    }
	  else
	    break;
	}

      offset = p - buf;
      /* offset == 1 means root directory.  */
      if (offset == 1)
	{
	  /* Include leading slash.  */
	  offset = 0;
	  break;
	}
    }
  free (buf);
  buf3 = xstrdup (buf2 + offset);
  free (buf2);

#ifdef __CYGWIN__
  if (st.st_dev != (DEV_CYGDRIVE_MAJOR << 16))
    {
      /* Reached some mount point not below /cygdrive.
	 GRUB does not know Cygwin's emulated mounts,
	 convert to Win32 path.  */
      grub_util_info ("Cygwin path = %s\n", buf3);
      char * temp = get_win32_path (buf3);
      free (buf3);
      buf3 = temp;
    }
#endif

  /* Remove trailing slashes, return empty string if root directory.  */
  len = strlen (buf3);
  while (len > 0 && buf3[len - 1] == '/')
    {
      buf3[len - 1] = '\0';
      len--;
    }

  return buf3;
}

#ifdef GRUB_UTIL
void
grub_util_init_nls (void)
{
#if (defined(ENABLE_NLS) && ENABLE_NLS)
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif /* (defined(ENABLE_NLS) && ENABLE_NLS) */
}
#endif
