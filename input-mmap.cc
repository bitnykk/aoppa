/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * MMAP input plugin.
 *
 * $Id: input-mmap.cc 1111 2009-03-04 07:57:14Z os $
 *
 * Copyright (C) 2003-2009 Oskari Saarenmaa <auno@auno.org>.
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

#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"


int
parse_file(int patch, int type, const char *fn)
{
  int fd;
  int cnt_all = 0, cnt_new = 0;
  struct stat st;
  time_t tstart, tend, telap;
  long size, off, last_mad=0;
  void *mp;
  unsigned char *data;

  fd = open(fn, O_RDONLY);
  if (fd < 0)
    {
      debugf(DEBUG_SERIOUS, "Failed to open %s for reading (%s)\n", fn, strerror(errno));
      exit(1);
    }

  fstat(fd, &st);
  size = st.st_size;

  mp = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  data = (unsigned char*)mp;

  tstart = time(NULL);
  debugf(1, "Reading records from %s (%d, %d)\n", fn, patch, type);
  for (off=0; off<size; data=(unsigned char *)mp+off)
    {
      size_t len;
      int aoid;

      if (off-last_mad > 1024*1024)
        {
          madvise(mp, off, MADV_DONTNEED);
          last_mad = off;
        }

      if (memcmp(data, "AOID: ", 6) != 0)
        break;
      aoid = strtol((char*)(data+=6), (char**)&data, 10);
      len = strtoul((char*)(data+=5), (char**)&data, 10);
      data += 12; /* skip ahead to our data chunk */
      cnt_new += handle_item(patch, type, aoid, len, data);
      cnt_all ++;

      off = (data + len + 12) - (unsigned char *)mp;
    }
  debugf(1, "Ok, read total %d records, %d were new / modified\n", cnt_all, cnt_new);

  munmap(mp, size);
  close(fd);

  tend = time(NULL);

  telap = tend-tstart;
  if(telap < 1)
    telap = 1;
  debugf(1, "%d seconds elapsed, %d (%d/s) records processed\n",
    telap, cnt_all, cnt_all/telap);

  return cnt_new;
}

aoppa_plugin_t aoppa_plugin =
{
  AOPPA_API_VERSION,
  AOPPA_INPUT,
  NULL,
  NULL,
  NULL,
  NULL,
  parse_file,
  "MMAP input plugin"
};
