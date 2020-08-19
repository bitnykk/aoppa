/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Miscellanous functions.
 *
 * $Id: misc.c 1111 2009-03-04 07:57:14Z os $
 *
 * Copyright (C) 2002-2009 Oskari Saarenmaa <auno@auno.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "crc32.h"

int g_debug = 0;

unsigned int
aoppa_crc32(const unsigned char *data, size_t len)
{
  unsigned int crc = ~0;
  unsigned char *p=(unsigned char*)data;
  while(len--)
    CRC32(crc, *p++);
  return crc;
}

void
debugf(int level, const char *fmt, ...)
{
  va_list ap;

  if(g_debug < level)
    return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fflush(stdout);
}

#ifndef HAVE_MEMMEM
void *
memmem(const void *in,   size_t in_len,
       const void *what, size_t what_len)
{
  void *p;

  for (;;)
  {
    if(what_len > in_len)
      return NULL;
    p = memchr(in, *(unsigned char *)what, in_len);
    if(!p)
      return NULL;
    in_len -= (unsigned char *)p - (unsigned char *)in;
    if(what_len > in_len)
      return NULL;
    if(!memcmp(p, what, what_len))
      return p;
    in = (unsigned char *)p + 1;
    in_len --;
  }
}
#endif /* !HAVE_MEMMEM */

#ifndef HAVE_STRNDUP
char *
strndup(const char *in, unsigned int in_len)
{
  char *res;

  res = (char*)malloc(in_len+1);
  memcpy(res, in, in_len);
  res[in_len] = 0;

  return res;
}
#endif /* !HAVE_STRNDUP */

#ifndef HAVE_ASPRINTF
int
asprintf(char **p, const char *fmt, ...)
{
  int res = -1;
  int ssz = 32;
  char *str, *restr;
  va_list ap;

  str = (char*)malloc(ssz);
  if(!str)
  {
    *p = NULL;
    return -1;
  }

  va_start(ap, fmt);
  for(;;)
  {
    res = vsnprintf(str, ssz, fmt, ap);
    if(res >= 0 && res < ssz)
      break;
    ssz *= 2;
    restr = (char*)realloc(str, ssz);
    if(!restr)
    {
      free(str);
      str = NULL;
      res = -1;
      break;
    }
    str = restr;
  }
  va_end(ap);

  *p = str;
  return res;
}
#endif /* !HAVE_ASPRINTF */
