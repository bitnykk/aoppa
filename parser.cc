/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * The resourcedatabase dump parser.
 *
 * $Id: parser.cc 1111 2009-03-04 07:57:14Z os $
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"

static void   load_effectkeys(void);
static void   parse_item(item_t *, unsigned char *, size_t);
static inline unsigned char * do_item_func(unsigned char *, size_t, item_t *, int, int);
static inline unsigned char * parse_reqs(unsigned char *, size_t, std::list<req_t>*, int, int);
static inline unsigned char * get_text(unsigned char *, size_t, char **);

// ao
static std::map<int,unsigned int> hashmap;
static std::map<int,aoid_ql_t> nanomap;
static std::map<uint16_t,uint8_t> effectkeys;
static std::vector<plugin_store_t*> storefuns;

void
init_parser(std::vector<plugin_store_t*> astore)
{
  load_effectkeys();
  storefuns = astore;
}

void
destroy_parser(void)
{
  effectkeys.clear();
  hashmap.clear();
  nanomap.clear();
}

static void
load_effectkeys(void)
{
  FILE *fp;

  fp = fopen("effectkeys.txt", "r");
  if(fp == NULL)
  {
    fprintf(stderr, "Unable to open effectkeys.txt for reading: %s\n", strerror(errno));
    exit(1);
  }
  for(;;)
  {
    char buf[0xff], *spp;
    uint16_t key;
    uint8_t  val;

    if(fgets(buf, sizeof buf, fp) == NULL)
      break;

    if(strlen(buf) < 4 || (spp = strchr(buf, ' ')) == NULL || *buf == '#' || *buf == ';' || *buf == '/')
      continue;

    key = (uint16_t)strtoul(buf, NULL, 0);
    val = (uint8_t) strtoul(spp, NULL, 0);

    if(val > AODB_ITEM_EFF_MAXVALS)
    {
      fprintf(stderr, "Maximum amount of effect values is %d (tried to set %d for key 0x%2x)\n", AODB_ITEM_EFF_MAXVALS, val, key);
      fprintf(stderr, "Edit parser.h and recompile everything if you need to raise the limit\n");
      exit(1);
    }

    effectkeys[key] = val;
  }
  fclose(fp);
}

void
set_item_hash(int id, unsigned int hash)
{
  hashmap[id] = hash;
}

void
set_nano_crystal(int nano, int crystal, int ql)
{
  nanomap[nano].aoid = crystal;
  nanomap[nano].ql   = ql;
}

int
handle_item(int patch, int type, int aoid, size_t len, unsigned char *data)
{
  item_t item;
  unsigned int hash = aoppa_crc32(data, len);

  debugf(DEBUG_INFO, "Entry: %d, hash: %08x", aoid, hash);
  debugf(DEBUG_INFO+1, ", hashmap: %08x", hashmap[aoid]);
  debugf(DEBUG_INFO, "\n");
  if(hashmap[aoid] == hash)
    return 0;
  hashmap[aoid] = hash;

  /* Initialize the data structure .. that memset call is Ugly */
  memset(&item, 0, (size_t)&item.attmap - (size_t)&item);
  item.aoid     = aoid;
  item.patch    = patch;
  item.metatype = (type == AOPT_NANO) ? 'n' : 'i';
  item.hash     = hash;

  parse_item(&item, data, len);
  if(item.metatype == -1)
  {
    debugf(DEBUG_SERIOUS, "Item %d parsing failed.\n", aoid);
    goto free_parse_item;
  }

  if(item.metatype == 'i' && item.type == 1)
    if(item.slot & 0x40 || item.slot & 0x100 || item.other.find(0x1b8) != item.other.end())
      item.metatype = 'w';

  if(item.metatype == 'n')
  {
    item.ncu = item.ql;
    std::map<int,aoid_ql_t>::const_iterator cryi;
    if((cryi = nanomap.find(item.aoid)) == nanomap.end())
    {
      item.ql = -1;
      item.crystal = -1;
    }
    else
    {
      item.ql = cryi->second.ql;
      item.crystal = cryi->second.aoid;
    }
  }

  for (std::vector<plugin_store_t*>::iterator storei=storefuns.begin();
       storei != storefuns.end();
       storei++)
    {
      (**storei)((void*)&item);
    }

  debugf(DEBUG_INFO-1, "New/modified entry: %d\n", aoid);

  free_parse_item:
  /* Destroy the data structure */
  if(item.name != NULL)
    free(item.name);
  if(item.info != NULL)
    free(item.info);
  for(std::list<eff_t>::iterator txi=item.effs.begin(); txi!=item.effs.end(); txi++)
    if(txi->text != NULL)
      free(txi->text);

  return 1;
}


#define NEXT(v)     do { memcpy(&v, ((uint32_t*)p), 4); p+= 4; } while(0)
#define NEXTV(v)    uint32_t v; NEXT(v);
#define REMAINING   (len-(p-data))
#define PARSEFAIL() do { item->metatype = -1; return; } while(0)
#define DATAFAIL(x) do { debugf(DEBUG_SERIOUS, "Reached an unknown key (0x%x) in item %d body at 0x%x (%s: %d)\n", x, item->aoid, p-data, __FILE__, __LINE__); PARSEFAIL(); } while(0)


static void
parse_item(item_t *item, unsigned char *data, size_t len)
{
  unsigned char *p = data + 0x24;
  bool head = true;
  uint32_t ftype = 0; // we need bigger scope for this variable, as it is used for catching lingering functions

  while(p + 4 < data+len)
  {
    uint32_t key, val;

    if(head)
    {
      NEXT(key);
      NEXT(val);
      debugf(DEBUG_VERBOSE, "In head, got key 0x%x, val 0x%x\n", key, val);

      switch(key)
      {
        case 0x00 :
          debugf(DEBUG_INFO, "item->flags = %d\n", val);
          item -> flags = val;
          break;

        case 0x08 :
          debugf(DEBUG_INFO, "item->duration = %d\n", val);
          item -> duration = val;
          break;

        case 0x17 :
          debugf(DEBUG_INFO, "property count for item : %d\n", val);
          break;

        case 0x1e :
          debugf(DEBUG_INFO, "item->props = %d\n", val);
          item -> props = val;
          break;

        case 0x36 :
          debugf(DEBUG_INFO, "item->ql = %d\n", val);
          item -> ql = val;
          break;

        case 0x4a :
          debugf(DEBUG_INFO, "item->value = %d\n", val);
          item -> value = val;
          break;

        case 0x4b :
          debugf(DEBUG_INFO, "item->strain = %d\n", val);
          item -> strain = val;
          break;

        case 0x4c :
          debugf(DEBUG_INFO, "item->type = %d\n", val);
          item -> type = val;
          break;

        case 0x4f :
          debugf(DEBUG_INFO, "item->icon = %d\n", val);
          item -> icon = val;
          break;

        case 0x58 :
          debugf(DEBUG_INFO, "item->defslot = %d\n", val);
          item -> defslot = val;
          break;

        case 0x64 :
          debugf(DEBUG_INFO, "item->mareq = %d\n", val);
          item -> mareq = val;
          item -> other[key] = val;
          break;

        case 0x65 :
          debugf(DEBUG_INFO, "item->multim = %d\n", val);
          item -> multim = val;
          item -> other[key] = val;
          break;

        case 0x86 :
          debugf(DEBUG_INFO, "item->multir = %d\n", val);
          item -> multir = val;
          item -> other[key] = val;
          break;

        case 0xd2 :
          debugf(DEBUG_INFO, "item->trecharge = %d\n", val);
          item -> trecharge = val;
          break;

        case 0xd3 :
          debugf(DEBUG_INFO, "item->tequip = %d\n", val);
          item -> tequip = val;
          break;

        case 0xd4 :
          debugf(DEBUG_INFO, "item->clip = %d\n", val);
          item -> clip = val;
          break;

        case 0x11c :
          debugf(DEBUG_INFO, "item->dcrit = %d\n", val);
          item -> dcrit = val;
          break;

        case 0x11d :
          debugf(DEBUG_INFO, "item->dmax = %d\n", val);
          item -> dmax = val;
          break;

        case 0x11e :
          debugf(DEBUG_INFO, "item->dmin = %d\n", val);
          item -> dmin = val;
          break;

        case 0x11f :
          debugf(DEBUG_INFO, "item->range = %d\n", val);
          item -> range = val;
          break;

        case 0x126 :
          debugf(DEBUG_INFO, "item->tattack = %d\n", val);
          item -> tattack = val;
          break;

        case 0x12a :
          debugf(DEBUG_INFO, "item->slot = %d\n", val);
          item -> slot = val;
          break;

        case 0x176 :
          debugf(DEBUG_INFO, "item->tburst = %d\n", val);
          item -> tburst = val;
          item -> other[key] = val;
          break;

        case 0x177 :
          debugf(DEBUG_INFO, "item->tfullauto = %d\n", val);
          item -> tfullauto = val;
          item -> other[key] = val;
          break;

        case 0x195 :
          debugf(DEBUG_INFO, "item->school = %d\n", val);
          item -> school = val;
          break;

        case 0x197 :
          debugf(DEBUG_INFO, "item->nanocost = %d\n", val);
          item -> nanocost = val;
          break;

        case 0x1a4 :
          debugf(DEBUG_INFO, "item->atype = %d\n", val);
          item -> atype = val;
          break;

        case 0x1b4 :
          debugf(DEBUG_INFO, "item->dtype = %d\n", val);
          item -> dtype = val;
          break;

        case 0x1b8 :
          debugf(DEBUG_INFO, "item->initskill = %d\n", val);
          item -> initskill = val;
          item -> other[key] = val;
          break;

        case 0x15 :
          if(val == 0x21)
          {
            uint16_t lname, linfo;

            lname = *((uint16_t*)p); p += 2;
            linfo = *((uint16_t*)p); p += 2;

            item->name = (char*)malloc(lname+1);
            item->info = (char*)malloc(linfo+1);
            memcpy(item->name, p, lname);
            memcpy(item->info, p+lname, linfo);
            item->name[lname] = 0;
            item->info[linfo] = 0;

            p += lname + linfo;

            head = false;

            debugf(DEBUG_INFO, "Name: %s\n", item->name);
            // printf("ITEM: %06d %s\n", item->aoid, item->name);
          }
          else
          {
            item -> other[key] = val;
          }
          break;

        default :
          item -> other[key] = val;
          break;
      }
    }
    else
    {
      NEXT(key);

      debugf(DEBUG_VERBOSE, "In body, got key 0x%x\n", key);

      if(key == 0x0)
      {
        continue;
      }
      if(key == 0x2)
      {
        NEXT(ftype);
        NEXTV(count);
        count >>= 10;
        debugf(DEBUG_INFO, "item: have %d effects for a function of type 0x%x\n", count, ftype);
        while(count--)
        {
          int fkey = 0;
          while(fkey == 0)
            NEXT(fkey);
          if((fkey & 0xcf00) != 0xcf00)
            DATAFAIL(fkey);
          p = do_item_func(p, REMAINING, item, ftype, fkey & 0xff);
          if(p == NULL)
            return; /* Failed, stop item parsing */
        }
      }
      else if(key == 0x16)
      {
        NEXT(val);
        if(val != 0x24)
          DATAFAIL(key);
        NEXTV(count);
        count >>= 10;
        while(count--)
        {
          NEXTV(rhook);
          NEXTV(subcount);
          subcount >>= 10;
          p = parse_reqs(p, REMAINING, &(item -> reqs), subcount, rhook);
        }
      }
      else if(key == 0x4)
      {
        NEXT(val);
        if(val != 0x4)
          DATAFAIL(key);
        NEXTV(count);
        count >>= 10;
        while(count--)
        {
          std::map<int,int> m;
          NEXTV(fkey);
          NEXTV(subcount);
          subcount >>= 10;
          while(subcount--)
          {
            NEXTV(subkey);
            NEXTV(subval);
            m[subkey] = subval;
          }
          if(fkey == 0xc)
            item->attmap = m;
          else if(fkey == 0xd)
            item->defmap = m;
          else
            debugf(DEBUG_INFO, "item had a skillmap, but it was not att or def (0x%x)\n", fkey);
        }
      }
      else if(key == 0x6)
      {
        NEXT(val);
        if(val != 0x1b)
          DATAFAIL(key);
        NEXTV(count);
        count >>= 10;
        while(count--)
        {
          NEXTV(fkey); // no idea what these are.. usually there's fkey:0xc, fkey:0xd
          NEXTV(fval); // and this one is always zero
        }
      }
      else if(key == 0xe || key == 0x14)
      {
        NEXT(val);
        NEXTV(count1);
        count1 >>= 10;
        while(count1--)
        {
          NEXT(val);
          NEXTV(count2);
          count2 >>= 10;
          while(count2--)
            NEXT(val);
        }
      }
      else if(key == 0x17)
      {
        // gah, stop parsing item here, it's most likely full of shit
        //NEXT(val);
        //NEXTV(count);
        //p += 17 * (count >> 10); // ignore this.. shop inventory afaik
        return;
      }
      else if((key & 0xcf00) == 0xcf00) // lingering function?
      {
        debugf(DEBUG_INFO-1, "Found lingering function 0x%x in item %d\n", key, item->aoid);
        p = do_item_func(p, REMAINING, item, ftype, key & 0xff);
        if(p == NULL)
          PARSEFAIL(); /* Failed, stop item parsing */
      }
      else
      {
        DATAFAIL(key);
      }
    }
  }
}

static inline unsigned char *
do_item_func(unsigned char *data, size_t len, item_t *item, int ftype, int key)
{
  unsigned char *p = data;
  uint32_t xxx;
  eff_t eff;

  eff.hook   = ftype;
  eff.type   = key;
  eff.text   = NULL;
  memset(eff.values, 0, sizeof eff.values);

  debugf(DEBUG_INFO, "Item function 0xcf%02x\n", key);

  NEXT(xxx); if(xxx != 0) debugf(DEBUG_SERIOUS+1, "%d xxx == %d\n", __LINE__, xxx);
  NEXT(xxx); if(xxx != 4) debugf(DEBUG_SERIOUS+1, "%d xxx == %d\n", __LINE__, xxx);
  // p += 4*2; // always 0, 4

  NEXTV(cnt);
  if(cnt > 0)
  {
    debugf(DEBUG_INFO, "Item function got requirements, %d.\n", cnt);
    p = parse_reqs(p, REMAINING, &(eff.reqs), cnt, -1);
  }

  NEXT(eff.hits);
  NEXT(eff.delay);
  NEXT(eff.target);
  NEXT(xxx); if(xxx != 9) debugf(DEBUG_SERIOUS+1, "%d xxx == %d\n", __LINE__, xxx);
  // p += 4; // always 9
  eff.valuecount = 0;

  switch(key)
  {
    /* hack for nano uploads */
    case 0x1b : // upload nano [id]
      eff.target = 2;
      NEXT(eff.values[eff.valuecount++]);
      if(strstr(item->name, "Nano Crystal") == item->name ||
         strstr(item->name, "NanoCrystal")  == item->name ||
         strstr(item->name, "Nano Cube")    == item->name ||
         strstr(item->name, "Weird looking ") == item->name ||
         strstr(item->name, "Super Charged Nano Crystal") == item->name ||
         strstr(item->name, ": Startup Crystal - ") != NULL)
      {
        nanomap[eff.values[0]].aoid = item->aoid;
        nanomap[eff.values[0]].ql   = item->ql;
      }
      break;

    /* hack for various text-displaying effects */
    case 0x34 : // chat text, length, text
    case 0x3e : // unknown1 text, length, text
    case 0x41 : // floating text, length, text
    case 0x7b : // unknown2 text, length, text
    case 0x8c : // dual text, len, text, len, text
    case 0x8e : // text, 0, len, text
    case 0xda : // shadowlands 15.0.6-ep1 text
    case 0xe5 : // AI 15.8.0.1-test text
      if(key == 0x8e || key == 0x8c)
        p += 4;   // skip one
      else if(key == 0xe5)
        p += 4*3; // skip three

      if(key == 0x8c)
      {
        char *ta, *tb;
        ta = (char*)p; p = (unsigned char*)memchr(p, 0, REMAINING) + 5; // null and length
        tb = (char*)p; p = (unsigned char*)memchr(p, 0, REMAINING) + 5; // null and unknown byte
        eff.text = (char*)malloc(strlen(ta)+strlen(tb)+3);
        sprintf(eff.text, "%s, %s", ta, tb);
      }
      else
      {
        p = get_text(p, REMAINING, &eff.text);
      }

      if(key == 0x34)
        p += 4*5; // skip five
      else if(key == 0x41 || key == 0xda)
        p += 4;   // skip one
      else if(key == 0x7b)
        p += 4*2; // skip two
      break;
  }

  std::map<uint16_t,uint8_t>::const_iterator ekey = effectkeys.find(key);
  if(ekey == effectkeys.end())
  {
    debugf(DEBUG_SERIOUS, "UNKNOWN FUNCTION <0x%x> <ITEM: %d>\n", key, item->aoid);
    return NULL;
    // exit(0);
  }
  for(cnt=0; cnt<ekey->second; cnt++)
    NEXT(eff.values[eff.valuecount++]);
  item->effs.push_back(eff);

  return p;
}

static inline unsigned char *
get_text(unsigned char *data, size_t len, char **textp)
{
  unsigned char *p = data;

  NEXTV(tlen);
  if(tlen > len)
  {
    debugf(DEBUG_SERIOUS, "Buffer underrun trying to get %d bytes of text!\n", tlen);
    *textp = NULL;
    return data + len;
  }
  debugf(DEBUG_INFO, "Getting %d bytes of text\n", tlen);
  *textp = (char*)malloc(tlen+1);
  memcpy(*textp, p, tlen);
  (*textp)[tlen] = 0;
  p += tlen;

  return p;
}

static inline unsigned char *
parse_reqs(unsigned char *data, size_t len, std::list<req_t> *reqlist, int cnt, int rhook)
{
  req_t req;
  unsigned char *p = data, opmod=0, havereq=0, reqnum=0;

  memset(&req, 0, sizeof(req));

  debugf(DEBUG_INFO, "Parsing %d requirements of type %d\n", cnt, rhook);
  while(cnt--)
  {
    if(p+4*3 > data+len)
    {
      debugf(DEBUG_SERIOUS, "Buffer underrun while parsing requirements!\n");
      return data+len;
    }

    NEXTV(skill);
    NEXTV(count);
    NEXTV(op);
    debugf(DEBUG_INFO, "Got requirement, skill %d, count %d, operator %d\n", skill, count, op);
    if(skill == 0 && count == 0)
    {
      if(op == 0x12)
        opmod |= op;
      else
        req.opmod |= opmod|op;
    }
    else
    {
      if(havereq)
        reqlist->push_back(req);
      else
        havereq = 1;
      req = (req_t){reqnum++, rhook, skill, count, op, 0};
    }
  }
  if(havereq)
    reqlist->push_back(req);

  return p;
}

#undef NEXT
#undef NEXTV
#undef REMAINING
#undef PARSEFAIL
#undef DATAFAIL
