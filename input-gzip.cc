/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * GZIP/Zlib input plugin.
 *
 * $Id: input-gzip.cc 1111 2009-03-04 07:57:14Z os $
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
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <zlib.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"


static int
parse_file(int patch, int type, const char *fn)
{
  gzFile fp;
  int cnt_all = 0, cnt_new = 0;
  time_t tstart, tend, telap;

  fp = gzopen(fn, "rb");
  if(fp == NULL)
  {
    debugf(DEBUG_SERIOUS, "Failed to open %s for reading (%s)\n", fn, strerror(errno));
    exit(1);
  }

  tstart = time(NULL);
  debugf(1, "Reading records from %s (%d, %d)\n", fn, patch, type);
  for(;;)
  {
    unsigned char line[0xff], *p, *data;
    size_t len;
    int aoid;

    if(gzgets(fp, (char*)line, sizeof line) == NULL)
      break;
    if(memcmp(line, "AOID: ", 6) != 0)
      break;
    aoid = strtol((char*)(line+6), (char**)&p, 10);
    len = strtoul((char*)(p+5), NULL, 10);
    gzseek(fp, 11, SEEK_CUR);
    data = (unsigned char*)malloc(len);
    if(gzread(fp, data, len) != (int)len)
    {
      debugf(DEBUG_SERIOUS, "Failed to read %d bytes from %s\n", len, fn);
      break;
    }
    cnt_new += handle_item(patch, type, aoid, len, data);
    cnt_all ++;
    free(data);
    gzseek(fp, 12, SEEK_CUR);
  }
  debugf(1, "Ok, read total %d records, %d were new / modified\n", cnt_all, cnt_new);
  gzclose(fp);

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
  "GZIP input plugin"
};
