/* SANE - Scanner Access Now Easy.

   Copyright (C) 2006 Wittawat Yamwong <wittawat@web.de>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
 */
# include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>		/* pow(C90) */

#include <errno.h>		/* POSIX */
#include <sys/time.h>		/* gettimeofday(4.3BSD) */
#include <unistd.h>		/* usleep */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

extern const pixma_config_t pixma_mp150_devices[];
extern const pixma_config_t pixma_mp750_devices[];
extern const pixma_config_t pixma_mp730_devices[];

static const pixma_config_t *const pixma_devices[] = {
  pixma_mp150_devices,
  pixma_mp750_devices,
  pixma_mp730_devices,
  NULL
};

static pixma_t *first_pixma = NULL;
static time_t tstart_sec = 0;
static uint32_t tstart_usec = 0;
static int debug_level = 1;

#ifndef NDEBUG

static void
u8tohex (uint8_t x, char *str)
{
  static const char hdigit[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
'e', 'f' };
  str[0] = hdigit[(x >> 4) & 0xf];
  str[1] = hdigit[x & 0xf];
  str[2] = '\0';
}

static void
u32tohex (uint32_t x, char *str)
{
  u8tohex (x >> 24, str);
  u8tohex (x >> 16, str + 2);
  u8tohex (x >> 8, str + 4);
  u8tohex (x, str + 6);
}

void
pixma_hexdump (int level, const void *d_, unsigned len)
{
  const uint8_t *d = (const uint8_t *) (d_);
  unsigned ofs, c;
  char line[100];		/* actually only 1+8+1+8*3+1+8*3+1 = 61 bytes needed */

  if (level > debug_level)
    return;
  ofs = 0;
  while (ofs < len)
    {
      char *p;
      line[0] = ' ';
      u32tohex (ofs, line + 1);
      line[9] = ':';
      p = line + 10;
      for (c = 0; c != 16 && (ofs + c) < len; c++)
	{
	  u8tohex (d[ofs + c], p);
	  p[2] = ' ';
	  p += 3;
	  if (c == 7)
	    {
	      p[0] = ' ';
	      p++;
	    }
	}
      p[0] = '\0';
      pixma_dbg (level, "%s\n", line);
      ofs += c;
    }
}

static int
time2str (char *buf, unsigned size)
{
  time_t sec;
  uint32_t usec;

  pixma_get_time (&sec, &usec);
  sec -= tstart_sec;
  if (usec >= tstart_usec)
    {
      usec -= tstart_usec;
    }
  else
    {
      usec = 1000000 + usec - tstart_usec;
      sec--;
    }
  return snprintf (buf, size, "%lu.%03u", (unsigned long) sec,
		   (unsigned) (usec / 1000));
}

void
pixma_dump (int level, const char *type, const void *data, int len,
	    int size, int max)
{
  int actual_len, print_len;
  char buf[20];

  if (level > debug_level)
    return;
  if (debug_level >= 20)
    max = -1;			/* dump every bytes */

  time2str (buf, sizeof (buf));
  pixma_dbg (level, "%s T=%s len=%d\n", type, buf, len);

  actual_len = (size >= 0) ? size : len;
  print_len = (max >= 0 && max < actual_len) ? max : actual_len;
  if (print_len >= 0)
    {
      pixma_hexdump (level, data, print_len);
      if (print_len < actual_len)
	pixma_dbg (level, " ...\n");
    }
  if (len < 0)
    pixma_dbg (level, "  ERROR: %s\n", strerror (-len));
  pixma_dbg (level, "\n");
}


#endif /* NDEBUG */

void
pixma_set_debug_level (int level)
{
  debug_level = level;
}

void
pixma_set_be16 (uint16_t x, uint8_t * buf)
{
  buf[0] = x >> 8;
  buf[1] = x;
}

void
pixma_set_be32 (uint32_t x, uint8_t * buf)
{
  buf[0] = x >> 24;
  buf[1] = x >> 16;
  buf[2] = x >> 8;
  buf[3] = x;
}

uint16_t
pixma_get_be16 (const uint8_t * buf)
{
  return ((uint16_t) buf[0] << 8) | buf[1];
}

uint32_t
pixma_get_be32 (const uint8_t * buf)
{
  return ((uint32_t) buf[0] << 24) + ((uint32_t) buf[1] << 16) +
    ((uint32_t) buf[2] << 8) + buf[3];
}

uint8_t
pixma_sum_bytes (const void *data, unsigned len)
{
  const uint8_t *d = (const uint8_t *) data;
  unsigned i, sum = 0;
  for (i = 0; i != len; i++)
    sum += d[i];
  return sum;
}

void
pixma_sleep (unsigned long usec)
{
  usleep (usec);
}

void
pixma_get_time (time_t * sec, uint32_t * usec)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  if (sec)
    *sec = tv.tv_sec;
  if (usec)
    *usec = tv.tv_usec;
}

int
pixma_map_status_errno (unsigned status)
{
  switch (status)
    {
    case PIXMA_STATUS_OK:
      return 0;
    case PIXMA_STATUS_FAILED:
      return -ECANCELED;
    case PIXMA_STATUS_BUSY:
      return -EBUSY;
    default:
      return -EPROTO;
    }
}

int
pixma_check_result (pixma_cmdbuf_t * cb)
{
  const uint8_t *r = cb->buf;
  unsigned header_len = cb->res_header_len;
  unsigned expected_reslen = cb->expected_reslen;
  int error;
  unsigned len;

  if (cb->reslen < 0)
    return cb->reslen;

  len = (unsigned) cb->reslen;
  if (len >= header_len)
    {
      error = pixma_map_status_errno (pixma_get_be16 (r));
      if (expected_reslen != 0)
	{
	  if (len == expected_reslen)
	    {
	      if (pixma_sum_bytes (r + header_len, len - header_len) != 0)
		error = -EPROTO;
	    }
	  else
	    {
	      /* This case will happen when a command cannot be completely
	         executed, e.g. because you press the cancel button. The
	         device will return only a header with PIXMA_STATUS_FAILED. */
	      if (len != header_len)
		error = -EPROTO;
	    }
	}
    }
  else
    error = -EPROTO;

#ifndef NDEBUG
  if (error == -EPROTO)
    {
      pixma_dbg (1, "WARNING: result len=%d expected %d: %s\n",
		 len, cb->expected_reslen, strerror (-error));
      pixma_hexdump (1, r, MIN (len, 64));
    }
#endif
  return error;
}

int
pixma_cmd_transaction (pixma_t * s, const void *cmd, unsigned cmdlen,
		       void *data, unsigned expected_len)
{
  int error, tmo;

  error = pixma_write (s->io, cmd, cmdlen);
  if (error != (int) cmdlen)
    return error;		/* FIXME: make sure that error < 0! */

  /* When you send the start_session command while the scanner optic is
     going back to the home position after the last scan session has been
     cancelled, you won't get the response before it arrives home. This takes
     about 5 seconds. If the last session was succeeded, the scanner will
     immediatly answer with PIXMA_STATUS_BUSY.

     Is 8 seconds timeout enough? This affects ALL commands that use
     pixma_cmd_transaction(). */
  tmo = 8;
  do
    {
      error = pixma_read (s->io, data, expected_len);
      if (error == -ETIMEDOUT)
	PDBG (pixma_dbg (2, "No response yet. Timed out in %d sec.\n", tmo));
    }
  while (error == -ETIMEDOUT && --tmo != 0);
  if (error < 0)
    {
      PDBG (pixma_dbg (1, "WARNING:Error in response phase. cmd:%02x%02x\n",
		       ((const uint8_t *) cmd)[0],
		       ((const uint8_t *) cmd)[1]));
      PDBG (pixma_dbg
	    (1,
	     "   If the scanner hangs, reset it and/or unplug the "
	     "USB cable.\n"));
    }
  return error;			/* length of the result packet or error */
}

uint8_t *
pixma_newcmd (pixma_cmdbuf_t * cb, unsigned cmd,
	      unsigned dataout, unsigned datain)
{
  unsigned cmdlen = cb->cmd_header_len + dataout;
  unsigned reslen = cb->res_header_len + datain;

  if (cmdlen > cb->size || reslen > cb->size)
    return NULL;
  memset (cb->buf, 0, cmdlen);
  cb->cmdlen = cmdlen;
  cb->expected_reslen = reslen;
  pixma_set_be16 (cmd, cb->buf);
  pixma_set_be16 (dataout + datain, cb->buf + cb->cmd_len_field_ofs);
  if (dataout != 0)
    return cb->buf + cb->cmd_header_len;
  else
    return cb->buf + cb->res_header_len;
}

int
pixma_exec (pixma_t * s, pixma_cmdbuf_t * cb)
{
  if (cb->cmdlen > cb->cmd_header_len)
    pixma_fill_checksum (cb->buf + cb->cmd_header_len,
			 cb->buf + cb->cmdlen - 1);
  cb->reslen =
    pixma_cmd_transaction (s, cb->buf, cb->cmdlen, cb->buf,
			   cb->expected_reslen);
  return pixma_check_result (cb);
}

int
pixma_exec_short_cmd (pixma_t * s, pixma_cmdbuf_t * cb, unsigned cmd)
{
  pixma_newcmd (cb, cmd, 0, 0);
  return pixma_exec (s, cb);
}

int
pixma_check_dpi (unsigned dpi, unsigned max)
{
  /* valid dpi = 75 * 2^n */
  unsigned temp = dpi / 75;
  if (dpi > max || dpi < 75 || 75 * temp != dpi || (temp & (temp - 1)) != 0)
    return -EINVAL;
  return 0;
}


int
pixma_init (void)
{
  PDBG (pixma_dbg (2, "pixma version %d.%d.%d\n", PIXMA_VERSION_MAJOR,
		   PIXMA_VERSION_MINOR, PIXMA_VERSION_BUILD));
  PASSERT (first_pixma == NULL);
  if (tstart_sec == 0)
    pixma_get_time (&tstart_sec, &tstart_usec);
  return pixma_io_init ();
}

void
pixma_cleanup (void)
{
  while (first_pixma)
    pixma_close (first_pixma);
  pixma_io_cleanup ();
}

int
pixma_open (unsigned devnr, pixma_t ** handle)
{
  int error;
  pixma_t *s;
  const pixma_config_t *cfg;

  *handle = NULL;
  cfg = pixma_get_device_config (devnr);
  if (!cfg)
    return -EINVAL;		/* invalid devnr */
  PDBG (pixma_dbg (2, "pixma_open(): %s\n", cfg->name));

  s = (pixma_t *) calloc (1, sizeof (s[0]));
  if (!s)
    return -ENOMEM;
  s->next = first_pixma;
  first_pixma = s;

  s->cfg = cfg;
  error = pixma_connect (devnr, &s->io);
  if (error < 0)
    {
      PDBG (pixma_dbg (2, "pixma_connect() failed:%s\n", strerror (-error)));
      goto rollback;
    }
  strncpy (s->id, pixma_get_device_id (devnr), sizeof (s->id));
  s->ops = s->cfg->ops;
  s->scanning = 0;
  error = s->ops->open (s);
  if (error < 0)
    goto rollback;
  *handle = s;
  return 0;

rollback:
  pixma_close (s);
  return error;
}

void
pixma_close (pixma_t * s)
{
  pixma_t **p;

  if (!s)
    return;
  for (p = &first_pixma; *p && *p != s; p = &((*p)->next))
    {
    }
  PASSERT (*p);
  if (!(*p))
    return;
  PDBG (pixma_dbg (2, "pixma_close(): %s\n", s->cfg->name));
  if (s->io)
    {
      if (s->scanning)
	{
	  PDBG (pixma_dbg (3, "pixma_close():scanning in progress, call"
			   " finish_scan()\n"));
	  s->ops->finish_scan (s);
	}
      s->ops->close (s);
      pixma_disconnect (s->io);
    }
  *p = s->next;
  free (s);
}

int
pixma_scan (pixma_t * s, pixma_scan_param_t * sp)
{
  int error;

  error = pixma_check_scan_param (s, sp);
  if (error < 0)
    return error;

#ifndef NDEBUG
  pixma_dbg (3, "\n");
  pixma_dbg (3, "pixma_scan(): start\n");
  pixma_dbg (3, "  line_size=%u image_size=%u channels=%u depth=%u\n",
	     sp->line_size, sp->image_size, sp->channels, sp->depth);
  pixma_dbg (3, "  dpi=%ux%u offset=(%u,%u) dimension=%ux%u\n",
	     sp->xdpi, sp->ydpi, sp->x, sp->y, sp->w, sp->h);
  pixma_dbg (3, "  gamma_table=%p source=%d\n", sp->gamma_table, sp->source);
#endif

  s->param = sp;
  s->cancel = 0;
  s->cur_image_size = 0;
  s->imagebuf.wptr = NULL;
  s->imagebuf.wend = NULL;
  s->imagebuf.rptr = NULL;
  s->imagebuf.rend = NULL;
  error = s->ops->scan (s);
  if (error >= 0)
    {
      s->scanning = 1;
    }
  else
    {
      PDBG (pixma_dbg (3, "pixma_scan() failed:%s\n", strerror (-error)));
    }

  return error;
}

int
pixma_read_image (pixma_t * s, void *buf, unsigned len)
{
  int result;
  pixma_imagebuf_t ib;

  if (!s->scanning)
    return 0;
  if (s->cancel)
    {
      result = -ECANCELED;
      goto cancel;
    }

  ib = s->imagebuf;		/* get rptr and rend */
  ib.wptr = (uint8_t *) buf;
  ib.wend = ib.wptr + len;
  while (ib.wptr != ib.wend)
    {
      if (ib.rptr == ib.rend)
	{
	  ib.rptr = ib.rend = NULL;
	  result = s->ops->fill_buffer (s, &ib);
	  if (result < 0)
	    goto cancel;
	  if (result == 0)
	    {			/* end of image? */
	      s->ops->finish_scan (s);
	      s->scanning = 0;
	      if (s->cur_image_size != s->param->image_size)
		{
		  PDBG (pixma_dbg (1, "WARNING:image size mismatches: "
				   "%u expected, %u received\n",
				   s->param->image_size, s->cur_image_size));
		}
	      PDBG (pixma_dbg (3, "pixma_read_image():completed\n"));
	      break;
	    }
	  s->cur_image_size += result;
	  PASSERT (s->cur_image_size <= s->param->image_size);
	}
      if (ib.rptr)
	{
	  unsigned count = MIN (ib.rend - ib.rptr, ib.wend - ib.wptr);
	  memcpy (ib.wptr, ib.rptr, count);
	  ib.rptr += count;
	  ib.wptr += count;
	}
    }
  s->imagebuf = ib;
  return ib.wptr - (uint8_t *) buf;

cancel:
  s->ops->finish_scan (s);
  s->scanning = 0;
  if (result == -ECANCELED)
    {
      PDBG (pixma_dbg (3, "pixma_read_image():cancelled by %sware\n",
		       (s->cancel) ? "soft" : "hard"));
    }
  return result;
}

void
pixma_cancel (pixma_t * s)
{
  s->cancel = 1;
}

int
pixma_enable_background (pixma_t * s, int enabled)
{
  return pixma_set_interrupt_mode (s->io, enabled);
}

unsigned
pixma_wait_event (pixma_t * s, int timeout /*ms */ )
{
  unsigned events;

  if (s->events == PIXMA_EV_NONE && s->ops->wait_event)
    s->ops->wait_event (s, timeout);
  events = s->events;
  s->events = PIXMA_EV_NONE;
  return events;
}

#define CLAMP2(x,w,min,max,dpi) do {		\
    unsigned m = (max) * (dpi) / 75;		\
    x = MIN(x, m - min);			\
    w = MIN(w, m - x);				\
    if (w < min)  w = min;			\
} while(0)

int
pixma_check_scan_param (pixma_t * s, pixma_scan_param_t * sp)
{
  if (!(sp->channels == 3 ||
	(sp->channels == 1 && (s->cfg->cap & PIXMA_CAP_GRAY) != 0)))
    return -EINVAL;

  if (pixma_check_dpi (sp->xdpi, s->cfg->xdpi) < 0 ||
      pixma_check_dpi (sp->ydpi, s->cfg->ydpi) < 0)
    return -EINVAL;

  /* xdpi must be equal to ydpi except that
     xdpi = max_xdpi and ydpi = max_ydpi. */
  if (!(sp->xdpi == sp->ydpi ||
	(sp->xdpi == s->cfg->xdpi && sp->ydpi == s->cfg->ydpi)))
    return -EINVAL;

  /* FIXME: I assume the same minimum width and height for every model. */
  CLAMP2 (sp->x, sp->w, 13, s->cfg->width, sp->xdpi);
  CLAMP2 (sp->y, sp->h, 1, s->cfg->height, sp->ydpi);

  if (!(s->cfg->cap & PIXMA_CAP_ADF))
    sp->source = PIXMA_SOURCE_FLATBED;

  if (sp->depth == 0)
    sp->depth = 8;
  if ((sp->depth % 8) != 0)
    return -EINVAL;

  sp->line_size = 0;

  if (s->ops->check_param (s, sp) < 0)
    return -EINVAL;

  if (sp->line_size == 0)
    sp->line_size = sp->depth / 8 * sp->channels * sp->w;
  sp->image_size = sp->line_size * sp->h;
  return 0;
}

const char *
pixma_get_string (pixma_t * s, pixma_string_index_t i)
{
  switch (i)
    {
    case PIXMA_STRING_MODEL:
      return s->cfg->name;
    case PIXMA_STRING_ID:
      return s->id;
    case PIXMA_STRING_LAST:
      return NULL;
    }
  return NULL;
}

const pixma_config_t *
pixma_get_config (pixma_t * s)
{
  return s->cfg;
}

void
pixma_fill_gamma_table (double gamma, uint8_t * table, unsigned n)
{
  int i;
  double r_gamma = 1.0 / gamma;
  double out_scale = 255.0;
  double in_scale = 1.0 / (n - 1);

  for (i = 0; (unsigned) i != n; i++)
    {
      table[i] = (int) (out_scale * pow (i * in_scale, r_gamma) + 0.5);
    }
}

int
pixma_find_scanners (void)
{
  return pixma_collect_devices (pixma_devices);
}

const char *
pixma_get_device_model (unsigned devnr)
{
  const pixma_config_t *cfg = pixma_get_device_config (devnr);
  return (cfg) ? cfg->name : NULL;
}


int
pixma_get_device_status (pixma_t * s, pixma_device_status_t * status)
{
  if (!status)
    return -EINVAL;
  memset (status, 0, sizeof (*status));
  return s->ops->get_status (s, status);
}