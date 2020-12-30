/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Auno.org PostgreSQL output plugin.
 *
 * $Id: aunoorg-output.cc 1111 2009-03-04 07:57:14Z os $
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include </usr/include/postgresql/libpq-fe.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"

#define AOPF_RO   0x01
#define AOPF_IDX  0x80

static void aunoorg_new_finish(void);
static void aunoorg_new_store(void *);
static void load_database(void);
static int  flush_sqlmap(void);

static inline void copyescape(char *, char *, size_t);
static inline PGresult *execf(const char *, ...);

struct sqlmap_line
{
  struct sqlmap_line *next;
  char *line;
};

static struct sqlmap
{
  char *name;
  size_t line_count;
  struct sqlmap_line *lines;
  struct sqlmap_line *last_line;
  struct sqlmap *next;
} *sqlmap_list = NULL;

static std::map<int,int> idpatch;
static std::map<int,int> noncurrent;
static std::map<int,int> patchadded;

static PGconn *dbconn;
static int options, reqid, xid;

static void
auno_init(config_vars_t *cfg)
{
  config_vars_t::const_iterator cfgval;
  const char *dbname, *dbuser, *userpass,*tmp;

  options = reqid = xid = 0;

  if(((cfgval = cfg->find("ro")) != cfg->end()) && cfgval->second == "yes")
  {
    debugf(1, "Read-only mode.\n");
    options |= AOPF_RO;
  }

  dbname = aoppa_get_config(cfg, "dbname");
  dbuser = aoppa_get_config(cfg, "dbuser");
  userpass = aoppa_get_config(cfg, "userpass");

  if((tmp = aoppa_get_config(cfg, "xid")))
  {
    xid = atoi(tmp);
  }

  if(dbname && dbuser && userpass)
  {
    char connstr[0xff];
    snprintf(connstr, 0xff, "dbname=%s user=%s password=%s", dbname, dbuser, userpass);
    dbconn = PQconnectdb(connstr);
    if(dbconn == NULL)
    {
      debugf(0, "Database structure allocation failed, out of memory?\n");
      exit(1);
    }
    if(PQstatus(dbconn) != CONNECTION_OK)
    {
      debugf(0, "Database login failed.\n");
      exit(1);
    }
  }
  else
  {
    debugf(0, "Auno.org-database plugin requires valid pgsql login credentials.\n");
    exit(1);
  }

  execf("set client_encoding='iso-8859-15'");

  load_database();
}

static void
load_database(void)
{
  PGresult *q;
  int rows, i;

  debugf(1, "Loading hashes from the database...");
  debugf(5, "\n");

  q = execf("SELECT aoid, hash, patchadded FROM aodb WHERE current");
  rows = PQntuples(q);
  if(!rows)
  {
    debugf(1, "the database is empty.\n");
    PQclear(q);
    options |= AOPF_IDX;
    return;
  }

  for(i=0; i<rows; i++)
  {
    int id  = atoi(PQgetvalue(q, i, 0)),
        sum = atoi(PQgetvalue(q, i, 1)),
        pad = atoi(PQgetvalue(q, i, 2));

    debugf(5, "READ HASH: %d -> %08x [%d]\n", id, sum, pad);
    set_item_hash(id, (unsigned int)sum);
    noncurrent[id] = 0;
    patchadded[id] = pad;
  }

  PQclear(q);

  debugf(5, "OK, read ");
  debugf(1, "%d\n", rows);

  debugf(1, "Loading nano crystals from the database...");
  debugf(5, "\n");

  q = execf("SELECT A.aoid, N.crystal, C.ql FROM aodb A JOIN aodb_nano N ON (A.current AND A.xid = N.xid and N.crystal != -1) JOIN aodb C ON (C.current AND C.aoid = N.crystal)");
  rows = PQntuples(q);
  for(i=0; i<rows; i++)
  {
    int nano    = atoi(PQgetvalue(q, i, 0)),
        crystal = atoi(PQgetvalue(q, i, 1)),
        ql      = atoi(PQgetvalue(q, i, 2));

    debugf(5, "OBJECT %d (QL %d) has nano effect %d\n", crystal, ql, nano);
    set_nano_crystal(nano, crystal, ql);
  }
  PQclear(q);

  debugf(5, "OK, read ");
  debugf(1, "%d\n", rows);

  q = execf("SELECT MAX(xid) FROM aodb");
  if (PQntuples(q))
    xid = atoi(PQgetvalue(q, 0, 0));
  PQclear(q);
  debugf(1, "High XID is %d\n", ++xid);

  q = execf("SELECT MAX(reqid) FROM aodb_eff");
  if (PQntuples(q))
    reqid = atoi(PQgetvalue(q, 0, 0));
  PQclear(q);
  debugf(1, "High REQID is %d\n", reqid);
}

/* Return a pointer to the requested sqlmap, allocate it if not found. */
static struct sqlmap *
sqlmap_open(const char *table)
{
  struct sqlmap *map = sqlmap_list, *last=NULL;

  while (map)
    {
      if (strcmp(map->name, table) == 0)
        return map;
      last = map;
      map = map->next;
    }

  map = (struct sqlmap *)calloc(1, sizeof(*map));
  map->name = strdup(table);
  if (last)
    last->next = map;
  else
    sqlmap_list = map;

  return map;
}

static void
sqlmap_append(struct sqlmap *map,
              const char *data)
{
  struct sqlmap_line *line;

  line = (struct sqlmap_line *) malloc(sizeof(*line));
  line->line = strdup(data);
  line->next = NULL;

  if (map->lines == NULL)
    {
      map->lines = line;
      map->last_line = line;
    }
  else
    {
      map->last_line->next = line;
      map->last_line = line;
    }

  map->line_count ++;
}

#define BLOB 0x4000
#define ESCAPEAPPEND(where, what, end) do \
        { \
          if(what != NULL) \
            copyescape(strrchr(where, 0), what, strlen(what)); \
          else \
            strcat(where, "\\N"); \
          strcat(where, end); \
        } while(0)

static void
aunoorg_new_store(void *itemp)
{
  char sql[BLOB+1];
  struct sqlmap *map1, *map2;
  item_t *item = (item_t*)itemp;

  if(noncurrent.find(item->aoid) != noncurrent.end())
    noncurrent[item->aoid] = item->patch;
  else
    noncurrent[item->aoid] = 0;

  if(patchadded[item->aoid] == 0)
    patchadded[item->aoid] = item->patch;

  xid ++;

  snprintf(sql, BLOB,
    "%d\t%d\t%c\t"
    "%d\t%d\t%c\t"
    "%d\t%d\t%d\t"
    "%d\t%d\t%d\t",
      xid,         (int)(item->hash),   item->metatype,
      item->aoid,  item->patch,  't',
      item->flags, item->props,  item->ql,
      item->type,  item->slot,   patchadded[item->aoid]);
  ESCAPEAPPEND(sql, item->name, "\t");
  ESCAPEAPPEND(sql, item->info, "\n");
  debugf(7, "AODB: %s", sql);
  sqlmap_append(sqlmap_open("aodb"), sql);

  snprintf(sql, BLOB,
    "%d\t%d\t%d\t"
    "%d\t%d\t%d\t"
    "%d\t%d\t%d\t"
    "%d\t%d\t%d\t"
    "%d\t%d\t%d\n",
      xid,             item->icon,     item->defslot,
      item->value,     item->tequip,   item->tattack,
      item->trecharge, item->dmin,     item->dmax,
      item->dcrit,     item->dtype,    item->clip,
      item->atype,     item->duration, item->range);
  debugf(7, "AODB_EXT: %s", sql);
  sqlmap_append(sqlmap_open("aodb_ext"), sql);

  if (item->metatype == 'n')
    {
      snprintf(sql, BLOB,
        "%d\t%d\t%d\t"
        "%d\t%d\t%d\n",
          xid,            item->crystal, item->ncu,
          item->nanocost, item->school,  item->strain);
      debugf(7, "AODB_NANO: %s", sql);
      sqlmap_append(sqlmap_open("aodb_nano"), sql);
    }

  /* Open both req and eff maps here, eff first, so it is always stored first
   * as rows in req might reference eff.
   */
  map1 = sqlmap_open("aodb_eff");
  map2 = sqlmap_open("aodb_req");

  debugf(8, "Starting to store requirements\n");
  std::list<req_t>::iterator reqi;
  for (reqi=item->reqs.begin(); reqi!=item->reqs.end(); reqi++)
    {
      snprintf(sql, BLOB, "%d\t\\N\t%d\t%d\t%d\t%d\t%d\t%d\n",
        xid, reqi->id, reqi->type, reqi->attribute, reqi->count, reqi->op, reqi->opmod);
      debugf(7, "AODB_REQ: %s", sql);
      sqlmap_append(map2, sql);
    }

  debugf(8, "Starting to store effects\n");
  std::list<eff_t>::iterator effi;
  for (effi=item->effs.begin(); effi!=item->effs.end(); effi++)
    {
      char my_reqid[32];
      const char *reqidp = "\\N";

      reqi = effi->reqs.begin();
      if (reqi != effi->reqs.end())
        {
          snprintf(my_reqid, sizeof(my_reqid), "%d", (int) ++reqid);
          reqidp = my_reqid;
          for (; reqi!=effi->reqs.end(); reqi++)
            {
              snprintf(sql, BLOB, "\\N\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n",
                reqidp, reqi->id, reqi->type, reqi->attribute, reqi->count, reqi->op, reqi->opmod);
              debugf(7, "AODB_REQ: %s", sql);
              sqlmap_append(map2, sql);
            }
        }

      snprintf(sql, BLOB, "%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t",
        xid,             effi->hook,      effi->type,
        reqidp,          effi->hits,      effi->delay,
        effi->target,    effi->values[0], effi->values[1],
        effi->values[2], effi->values[3], effi->values[4]);
      ESCAPEAPPEND(sql, effi->text, "\n");
      debugf(7, "AODB_EFF: %s", sql);
      sqlmap_append(map1, sql);
    }

  debugf(8, "Starting to store 'other' data\n");
  std::map<int,int>::iterator iii;
  map1 = sqlmap_open("aodb_other");
  for(iii=item->other.begin(); iii!=item->other.end(); iii++)
  {
    snprintf(sql, BLOB, "%d\t%d\t%d\n",
      xid, iii->first, iii->second);
    debugf(7, "AODB_OTHER: %s", sql);
    sqlmap_append(map1, sql);
  }

  debugf(8, "Starting to store skills/att\n");
  map1 = sqlmap_open("aodb_skill");
  for(iii=item->attmap.begin(); iii!=item->attmap.end(); iii++)
  {
    snprintf(sql, BLOB, "%d\t%d\t%d\t%d\n",
      xid, 'a', iii->first, iii->second);
    debugf(7, "AODB_SKILLS[a]: %s", sql);
    sqlmap_append(map1, sql);
  }

  debugf(8, "Starting to store skills/def\n");
  for(iii=item->defmap.begin(); iii!=item->defmap.end(); iii++)
  {
    snprintf(sql, BLOB, "%d\t%d\t%d\t%d\n",
      xid, 'd', iii->first, iii->second);
    debugf(7, "AODB_SKILLS[d]: %s", sql);
    sqlmap_append(map1, sql);
  }

  debugf(6, "Done storing item.\n");
}

static int
flush_sqlmap(void)
{
  int cnt = 0;
  struct sqlmap *map=sqlmap_list;
  struct sqlmap_line *line=NULL;
  void *foo;

  debugf(1, "Entering tables in the database.. ");

  while (map)
  {
    /* Skip empty tables. */
    if (map->line_count <= 0)
      goto next_map;

    debugf(1, "%s:%ld ", map->name, map->line_count);
    debugf(5, "\n");

    if (!(options & AOPF_RO))
      execf("COPY %s FROM stdin", map->name);

    line = map->lines;
    while (line)
    {
      debugf(5, "Putting line: %s", line->line);
      if(!(options & AOPF_RO))
        PQputline(dbconn, line->line);
      cnt ++;
      free(line->line);
      foo = line;
      line = line->next;
      free(foo);
    }

    if (!(options & AOPF_RO))
    {
      PQputline(dbconn, "\\.\n");
      PQendcopy(dbconn);
    }

   next_map:
    foo = map;
    free(map->name);
    map = map->next;
    free(foo);
  }

  sqlmap_list = NULL;

  debugf(1, ".. done\n");

  return cnt;
}

static void
aunoorg_new_finish(void)
{
  int cnt = 0;
  char tmp[0xff];
  struct sqlmap_line *update_sql = NULL;
  std::map<int,int>::iterator mi;

  for(mi=noncurrent.begin(); mi!=noncurrent.end(); mi++)
  {
    if(mi->second > 0)
    {
      sprintf(tmp, "UPDATE aodb SET current=false WHERE current AND aoid=%d AND patch<%d",
        mi->first, mi->second);
      debugf(DEBUG_INFO, "SQL: %s\n", tmp);
      if(!(options & AOPF_RO))
      {
        struct sqlmap_line *update_line = (struct sqlmap_line *)malloc(sizeof(struct sqlmap_line));
        update_line->line = strdup(tmp);
        update_line->next = update_sql;
        update_sql = update_line;
      }
      cnt ++;
    }
  }
  noncurrent.clear();

  if(cnt == 0)
  {
    debugf(1, "The database's current fields do not need updating.\n");
    PQfinish(dbconn);
    return;
  }

  debugf(1, "Updating the database's 'current' fields %d times...", cnt);
  debugf(DEBUG_INFO, "\n");

  if(options & AOPF_IDX)
  {
    debugf(DEBUG_INFO, "Creating temporary index for aoid and patch.\n");
    if(!(options & AOPF_RO))
    {
      snprintf(tmp, sizeof tmp, "aoppa_tmp_idx_%ld", (long)time(NULL));
      execf("create index %s on aodb (aoid, patch)", tmp);
      execf("analyze aodb");
    }
  }

  if(!(options & AOPF_RO))
  {
    struct sqlmap_line *update_line = update_sql;
    while (update_line)
      {
        execf("%s", update_line->line);
        free(update_line->line);
        update_sql = update_line->next;
        free(update_line);
        update_line = update_sql;
      }
  }

  if(options & AOPF_IDX)
  {
    debugf(DEBUG_INFO, "Dropping temporary index for aoid and patch.\n");
    if(!(options & AOPF_RO))
      execf("drop index %s", tmp);
  }

  debugf(1, "done.\n");

  debugf(1, "Analyzing tables...");
  execf("analyze aodb");
  execf("analyze aodb_ext");
  execf("analyze aodb_req");
  execf("analyze aodb_nano");
  execf("analyze aodb_skill");
  execf("analyze aodb_other");
  debugf(1, "done.\n");

  PQfinish(dbconn);
}

static inline void
copyescape(char *to, char *from, size_t len)
{
  unsigned int i;
  for(i=0; i<len; i++)
  {
    if(*from == '\t' || *from == '\n')
    {
      *(to++) = '\\';
      *(to++) = *(from++) == '\n' ? 'n' : 't';
      continue;
    }
    if(*from == '\r')
    {
      from++;
      continue;
    }
    if(*from == '\\')
      *(to++) = '\\';
    *(to++) = *(from++);
  }
  *to = 0;
}

static inline PGresult *
execf(const char *fmt, ...)
{
  va_list ap;
  char *str;
  PGresult *q;

  va_start(ap, fmt);
  vasprintf(&str, fmt, ap);
  va_end(ap);

  q = PQexec(dbconn, str);
  switch(PQresultStatus(q))
  {
    case PGRES_BAD_RESPONSE :
    case PGRES_NONFATAL_ERROR :
    case PGRES_FATAL_ERROR :
      fprintf(stderr, "\n"
        "Database query failed! %s: %s\n"
        "Query: %s\n",
          PQresStatus(PQresultStatus(q)),
          PQresultErrorMessage(q),
          str);
      /* fall-through */

    case PGRES_COMMAND_OK :
      PQclear(q);
      q = NULL;

    default :
      break;
  }
  free(str);
  return q;
}

aoppa_plugin_t aoppa_plugin =
{
  AOPPA_API_VERSION,
  AOPPA_OUTPUT,
  auno_init,
  aunoorg_new_finish,
  aunoorg_new_store,
  flush_sqlmap,
  NULL,
  "Auno.org plugin"
};
