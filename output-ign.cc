/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * IGN bot itemdb output plugin.
 *
 * $Id: output-ign.cc 1111 2009-03-04 07:57:14Z os $
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

/*
 * This plugin outputs the itemdatabse in format suitable for Amona's
 * IGN bot.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"

static int records = 0;

static int
reset_records()
{
  int x = records;
  records = 0;
  return x;
}

static void
write_record(void *itemp)
{
  item_t *item = (item_t*)itemp;
  records ++;
  printf("%d\t%d\t%s\n",
    item->aoid,
    item->ql,
    item->name);
}

aoppa_plugin_t aoppa_plugin =
{
  AOPPA_API_VERSION,	// api version
  AOPPA_OUTPUT,		// type
  NULL,			// init
  NULL,			// finish
  write_record,		// store
  reset_records,	// flush
  NULL,			// parser
  "IGN output plugin"	// name
};
