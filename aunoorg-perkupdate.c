/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Auno.org PostgreSQL perk profession requirement table updater.
 *
 * $Id: aunoorg-perkupdate.c 1111 2009-03-04 07:57:14Z os $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include </usr/include/postgresql/libpq-fe.h>

static int
exec_and_info(PGconn *, const char *);

int
main(int argc, char **argv)
{
  PGconn *dbconn;
  char buf[0x200];
  const char *dbname, *dbuser, *userpass;

  if(argc != 4)
  {
    fprintf(stderr, "Usage: %s <dbname> <dbuser> <userpass>\n", argv[0]);
    exit(1);
  }

  userpass = (const char *)(argv[3]);  
  dbuser = (const char *)(argv[2]);
  dbname = (const char *)(argv[1]);

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

  PGresult *q = PQexec(dbconn, "select aoid, perkline, perklineid, perklevel from aodb_perk order by perkline asc, perklineid asc, perklevel asc");
  int i, rows = PQntuples(q);
  if(rows <= 0)
  {
    printf("No perks found!\n");
  }
  else
  {
    int lastprof=-1, lastlreq=0, lastplid=0;

    for(i=0; i<rows; i++)
    {
      int aoid = atoi(PQgetvalue(q, i, 0)),
          plvl = atoi(PQgetvalue(q, i, 3)),
          plid = atoi(PQgetvalue(q, i, 2));
      const char *line = PQgetvalue(q, i, 1);

      if (plid != lastplid)
        {
          lastprof = -1;
          lastlreq =  0;
          lastplid = plid;
        }

      snprintf(buf, sizeof buf, "select value from aodb_req where attribute = 60 and hook = 6 and type = 0 and xid = (select xid from aodb where current and aoid = %d)", aoid);
      PGresult *sq = PQexec(dbconn, buf);
      int lreq=0, breed=0, prof=0, si, srows = PQntuples(sq);
      if(srows == 0)
        prof = -1;
      else for(si=0; si<srows; si++)
        prof |= 1<<atoi(PQgetvalue(sq, si, 0));
      prof &= lastprof;
      PQclear(sq);

      snprintf(buf, sizeof buf, "select value from aodb_req where attribute = 54 and hook = 6 and type = 2 and xid = (select xid from aodb where current and aoid = %d)", aoid);
      sq = PQexec(dbconn, buf);
      srows = PQntuples(sq);
      if(srows == 1)
        lreq = atoi(PQgetvalue(sq, 0, 0))+1;
      if(lreq < lastlreq)
        lreq = lastlreq;
      PQclear(sq);

      snprintf(buf, sizeof buf, "select value from aodb_req where attribute = 4 and hook = 6 and type = 0 and xid = (select xid from aodb where current and aoid = %d)", aoid);
      sq = PQexec(dbconn, buf);
      srows = PQntuples(sq);
      if(srows == 1)
        breed = 1<<atoi(PQgetvalue(sq, 0, 0));
      else
        breed = -1;
      PQclear(sq);

      printf("Perk: %-30s (L%3d) : %08x : %08x : %03d\n", line, plvl, breed, prof, lreq);
      snprintf(buf, sizeof buf, "update aodb_perk set breedmask=%d, profmask=%d, levelreq=%d where aoid=%d", breed, prof, lreq, aoid);
      PQclear(PQexec(dbconn, buf));

      lastlreq = lreq;
      lastprof = prof;
    }
    printf("Updated %d rows to 'P'\n", exec_and_info(dbconn, "update aodb set metatype = 'P' where type = 2 and slot = 0 and metatype='i'"));
    printf("Updated %d rows to 'p'\n", exec_and_info(dbconn, "update aodb set metatype = 'p' where aodb.aoid = aodb_perk.aoid and aodb.metatype in ('P', 'i')"));
    printf("Updated %d rows to 'i'\n", exec_and_info(dbconn, "update aodb set metatype = 'i' where metatype = 'p' and aodb.aoid not in (select aoid from aodb_perk)"));
    PQclear(PQexec(dbconn, "vacuum full analyze aodb_perk"));
  }

  PQclear(q);
  PQfinish(dbconn);
  return 0;
}

static int
exec_and_info(PGconn *db, const char *sql)
{
  PGresult *q = PQexec(db, sql);
  char *res = PQcmdTuples(q);
  PQclear(q);
  return strlen(res) ? atoi(res) : 0;
}
