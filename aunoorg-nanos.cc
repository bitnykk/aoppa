/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Auno.org PostgreSQL nano table generator.
 *
 * $Id: aunoorg-nanos.cc 1111 2009-03-04 07:57:14Z os $
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include </usr/include/postgresql/libpq-fe.h>

#include <map>

static PGresult *
execf(PGconn *, const char *, ...);

int
main(int argc, char **argv)
{
  PGconn *dbconn;
  char buf[0x200];
  const char *dbname, *dbuser, *userpass;

  if(argc != 3)
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

  PGresult *q;
  int i, rows;
  std::map<int,int> strains;
  std::map<int,int> strains_force;

#define GETSTRAIN_FORCE(sql,force) \
    do { q = PQexec(dbconn, sql); \
      for(i=0, rows = PQntuples(q); i<rows; i++) { \
        strains[atoi(PQgetvalue(q,i,0))] = atoi(PQgetvalue(q,i,1)); \
        if (force) strains_force[atoi(PQgetvalue(q,i,0))] = force; \
      } \
      PQclear(q); \
    } while(0)
#define GETSTRAIN(sql) GETSTRAIN_FORCE((sql), 0)

  /* Assign logical strain -(3|8|12)(071|072|175) for engineer, crat and mp pets/items */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, aodb_req.value*-1000-aodb_eff.type "
              "FROM aodb, aodb_nano, aodb_eff, aodb_req "
             "WHERE aodb.xid = aodb_nano.xid "
               "AND aodb.xid = aodb_eff.xid AND aodb.xid = aodb_req.xid "
               "AND aodb_req.attribute IN (60, 368) AND aodb_req.value IN (8,12,3) "
               "AND aodb.current AND aodb_nano.school = 5 AND aodb_nano.crystal > 0 "
               "AND aodb_eff.type IN (71,72,175)");

  /* Assign logical strain -(6|10)20[13] for doctor and adventurer heals */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, -aodb_req.value*1000-200-aodb_eff.target "
              "FROM aodb, aodb_nano, aodb_eff, aodb_req "
             "WHERE aodb.xid = aodb_nano.xid "
               "AND aodb.xid = aodb_eff.xid AND aodb.xid = aodb_req.xid "
               "AND aodb_req.attribute IN (60, 368) AND aodb_req.value IN (6, 10) "
               "AND aodb.current AND aodb_nano.school = 2 AND aodb_nano.crystal > 0");

  /* Assign strains for nanos that cast other nanos with strains */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, cae_nano.strain "
              "FROM aodb, aodb_nano, aodb_nano cae_nano, aodb_eff, aodb caedb "
             "WHERE aodb.current AND aodb.xid = aodb_nano.xid "
               "AND aodb_nano.xid = aodb_eff.xid AND aodb_nano.strain = 0 "
               "AND aodb_eff.type IN (59, 74, 95, 97) AND caedb.aoid = aodb_eff.value1 "
               "AND caedb.current AND caedb.xid = cae_nano.xid "
               "AND cae_nano.strain != 0 AND aodb_nano.crystal > 0");

  /* NT Nukes (AOE) */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, -11106 "
              "FROM aodb, aodb_eff, aodb_nano, aodb_req AS aodb_req_prof "
             "WHERE aodb.xid = aodb_nano.xid AND aodb.xid = aodb_eff.xid AND aodb.current "
               "AND aodb.xid = aodb_req_prof.xid AND aodb_req_prof.attribute IN (60, 368) AND aodb_req_prof.value = 11 "
               "AND aodb_eff.type = 95 AND aodb_nano.school = 1 AND aodb_nano.crystal > 0");
  /* NT & Crat Nukes (DD) */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, aodb_req_prof.value*-1000-108 "
              "FROM aodb, aodb_eff, aodb_nano, aodb_req AS aodb_req_prof "
             "WHERE aodb.xid = aodb_nano.xid AND aodb.xid = aodb_eff.xid AND aodb.current "
               "AND aodb.xid = aodb_req_prof.xid AND aodb_req_prof.attribute IN (60, 368) AND aodb_req_prof.value IN (11, 8) "
               "AND aodb_eff.type = 10 AND aodb_nano.school = 1 AND aodb_nano.crystal > 0");
  /* NT Nukes (GA) */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, -11107 "
              "FROM aodb, aodb_req AS aodb_req_eff, aodb_eff, aodb_nano, aodb_req AS aodb_req_prof "
             "WHERE aodb.xid = aodb_nano.xid AND aodb.xid = aodb_eff.xid AND aodb.current "
               "AND aodb.xid = aodb_req_prof.xid AND aodb_req_prof.attribute IN (60, 368) AND aodb_req_prof.value = 11 "
               "AND aodb_req_eff.reqid = aodb_eff.reqid AND aodb_nano.school = 1 "
               "AND aodb_req_eff.attribute = 355 AND aodb_nano.crystal > 0");
  /* NT Nukes (Cyberdeck) */
  GETSTRAIN("SELECT DISTINCT ON (aodb.aoid) aodb.aoid, -11109 "
              "FROM aodb, aodb_req, aodb_nano, aodb_req AS aodb_req_prof, aodb_eff "
             "WHERE aodb.current AND aodb.xid = aodb_nano.xid AND aodb.xid = aodb_req.xid "
               "AND aodb.xid = aodb_req_prof.xid AND aodb.xid = aodb_eff.xid "
               "AND aodb_req_prof.attribute IN (60, 368) AND aodb_req_prof.value = 11 "
               "AND aodb_req.attribute = 355 AND aodb_nano.school = 1 AND aodb_nano.crystal > 0"
               "AND aodb_eff.type = 10 AND aodb_eff.value1 = 27");

  /* Grab manually set strains */
  GETSTRAIN_FORCE("SELECT aoid, strain FROM strain_nano", 1);

  q = PQexec(dbconn,
    "SELECT aodb      . xid    ,"
           "aodb      . aoid   ,"
           "aodb_ext  . icon   ,"
           "aodb_nano . school ,"
           "aodb_nano . strain ,"
           "aodb      . ql     ,"
           "aodb_nano . ncu    ,"
           "COALESCE(aodb_other.value, aodb_nano.ncu) AS stack, "
           "aodb      . name   ,"
           "aodb      . info    "
      "FROM aodb "
           "JOIN aodb_nano  ON (aodb.xid = aodb_nano.xid AND aodb.metatype = 'n' AND aodb.current AND aodb_nano.crystal > 0) "
           "JOIN aodb_ext   ON (aodb.xid = aodb_ext.xid) "
      "LEFT JOIN aodb_other ON (aodb.xid = aodb_other.xid AND aodb_other.attribute = 551) "
      "ORDER BY aoid asc");
  rows = PQntuples(q);
  if(rows <= 0)
  {
    printf("No nanos found!\n");
  }
  else
  {
    PQclear(PQexec(dbconn, "DROP TABLE nanos_tmp"));
    PQclear(PQexec(dbconn, "CREATE TABLE nanos_tmp (aoid int4, icon int4, school int2, strain int2, es int2, ql int2, ncu int2, stack int2, prof int2, lvl int2, mm int2, bm int2, pm int2, mc int2, ts int2, si int2, name text, info text)"));

    for(i=0; i<rows; i++)
    {
      int xid    = atoi(PQgetvalue(q, i, 0)),
          aoid   = atoi(PQgetvalue(q, i, 1)),
          icon   = atoi(PQgetvalue(q, i, 2)),
          school = atoi(PQgetvalue(q, i, 3)),
          strain = atoi(PQgetvalue(q, i, 4)),
          ql     = atoi(PQgetvalue(q, i, 5)),
          ncu    = atoi(PQgetvalue(q, i, 6)),
          stack  = atoi(PQgetvalue(q, i, 7)),
          prof=0, lvl=0, mm=0, bm=0, pm=0, mc=0, ts=0, si=0, skip=0, es=0;
      const char *name = PQgetvalue(q, i, 8),
                 *info = PQgetvalue(q, i, 9);
      char xname[0x4000], xinfo[0x4000];
      PGresult *xq = execf(dbconn, "SELECT attribute,value,type,opmod FROM aodb_req WHERE xid = %d AND hook = 3", xid);
      int xi, xrows = PQntuples(xq);
      for(xi=0; xi<xrows; xi++)
      {
        int attr  = atoi(PQgetvalue(xq, xi, 0)),
            type  = atoi(PQgetvalue(xq, xi, 2)),
            value = atoi(PQgetvalue(xq, xi, 1)) + (type == 2),
            opmod = atoi(PQgetvalue(xq, xi, 3));

        if ((opmod & 0x12) == 0x12) /* requirement is for target */
          continue;

        switch(attr)
        {
          case 0x3c : case 0x170 : prof = value; break;
          case 0x36 : lvl = value; break;
          case 0x7a : si = value; break;
          case 0x7f : mm = value; break;
          case 0x80 : bm = value; break;
          case 0x81 : pm = value; break;
          case 0x82 : mc = value; break;
          case 0x83 : ts = value; break;
          case 0x1c7 : skip++; break; // NPC
          case 0x185 : es = value; break;
        }
      }
      PQclear(xq);

      if(strains.find(aoid) != strains.end() && (strains[aoid] > 0 || strains[aoid] / -1000 == prof))
      {
        if(strain == 0 || strains_force.find(aoid) != strains_force.end())
        {
          strain = strains[aoid];
        }
      }
      else if(strain == 0)
      {
        strain = -school;
      }

      PQescapeString(xname, name, strlen(name));
      PQescapeString(xinfo, info, strlen(info));
      PQclear(execf(dbconn, "INSERT INTO nanos_tmp VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', '%s')",
        aoid, icon, school, strain, es, ql, ncu, stack, prof, lvl, mm, bm, pm, mc, ts, si, xname, xinfo));
      printf("%06d :: %s\n", aoid, name);
    }
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET lvl = NULL WHERE lvl = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET mm = NULL WHERE mm = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET bm = NULL WHERE bm = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET pm = NULL WHERE pm = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET mc = NULL WHERE mc = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET ts = NULL WHERE ts = 0"));
    PQclear(PQexec(dbconn, "UPDATE nanos_tmp SET si = NULL WHERE si = 0"));
    PQclear(PQexec(dbconn, "VACUUM FULL ANALYZE nanos_tmp"));
    PQclear(PQexec(dbconn, "DROP TABLE nanos"));
    PQclear(PQexec(dbconn, "ALTER TABLE nanos_tmp RENAME TO nanos"));
  }

  PQclear(q);
  PQfinish(dbconn);
  return 0;
}

static PGresult *
execf(PGconn *db, const char *fmt, ...)
{
  va_list ap;
  char *str;

  va_start(ap, fmt);
  vasprintf(&str, fmt, ap);
  va_end(ap);

  PGresult *q = PQexec(db, str);
  free(str);
  return q;
}
