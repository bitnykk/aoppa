/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Auno.org PostgreSQL patch reverting tool.
 *
 * $Id: aunoorg-revert.cc 1111 2009-03-04 07:57:14Z os $
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

// XXX: This is too slow to be used

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include </usr/include/postgresql/libpq-fe.h>

#include <list>

int
main(int argc, char **argv)
{
  PGconn *dbconn;
  char buf[0x200];
  const char *dbname, *dbuser, *userpass;
  int patch;
  std::list<int> edelete, erevert;
  std::list<int>::iterator li;

  if(argc != 5)
  {
    fprintf(stderr, "Usage: %s <dbuser> <dbname> <userpass> <patch>\n", argv[0]);
    exit(1);
  }

  dbuser = (const char *)(argv[1]);
  dbname = (const char *)(argv[2]);
  userpass = (const char *)(argv[3]);
  patch  = atoi(argv[4]);

  snprintf(buf, sizeof buf, "dbname=%s user=%s password=%s", dbname, dbuser, userpass);
  dbconn = PQconnectdb(buf);
  if(dbconn == NULL)
  {
    fprintf(stderr, "Database structure allocation failed, out of memory?\n");
    exit(1);
  }
  else if(PQstatus(dbconn) != CONNECTION_OK)
  {
    fprintf(stderr, "Database login failed.\n");
    exit(1);
  }

  snprintf(buf, sizeof buf, "select xid, aoid, current from aodb where patch = %d", patch);
  PGresult *q = PQexec(dbconn, buf);
  int i, rows = PQntuples(q);
  if(rows <= 0)
  {
    printf("No tuples found for that patch, nothing to do.\n");
    goto end;
  }

  for(i=0; i<rows; i++)
  {
    int xid = atoi(PQgetvalue(q, i, 0)),
       aoid = atoi(PQgetvalue(q, i, 1)),
        cur = strcmp(PQgetvalue(q, i, 2), "t") == 0;
    edelete.push_back(xid);
    if(cur)
    {
      snprintf(buf, sizeof buf, "select xid from aodb where aoid = %d and current is false order by patch desc limit 1", aoid);
      PGresult *sq = PQexec(dbconn, buf);
      if(PQntuples(sq) == 1)
      {
        erevert.push_back(atoi(PQgetvalue(sq, 0, 0)));
      }
      PQclear(sq);
    }
  }

  PQclear(q);

  printf("Delete (%ld).", edelete.size());
  for(li=edelete.begin(); li!=edelete.end(); li++)
  {
    snprintf(buf, sizeof buf, "delete from aodb where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_eff where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_ext where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_nano where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_other where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_skill where xid = %d", *li); PQclear(PQexec(dbconn, buf));
    snprintf(buf, sizeof buf, "delete from aodb_req where xid = %d", *li); PQclear(PQexec(dbconn, buf));
  }
  printf("Revert (%ld).", erevert.size());
  for(li=erevert.begin(); li!=erevert.end(); li++)
  {
    snprintf(buf, sizeof buf, "update aodb set current = true where xid = %d", *li); PQclear(PQexec(dbconn, buf));
  }

end:
  PQfinish(dbconn);
  return 0;
}
