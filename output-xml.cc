/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * XML output plugin.
 *
 * $Id: output-xml.cc 1111 2009-03-04 07:57:14Z os $
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
 * Another horrible mixture of C and C++ code.. ugh.
 *
 * This plugin generates XML output of the item structures handed over by
 * the parser/scanner, the format is quite 'loose'.. maybe someone will
 * create an AO item DTD some day :)
 *
 */

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

#include <sstream>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"

#ifndef OUTPUT_XML_MAP_FILENAME
#define OUTPUT_XML_MAP_FILENAME "output-xml.map"
#endif /* !OUTPUT_XML_MAP_FILENAME */

typedef struct store_xml_eff_descr_s
{
  std::ostringstream action;
  std::ostringstream attribute;
  std::ostringstream value;
  std::string target;
} eff_descr_t;

static std::map<int,int> idpatch;
static std::map<std::string, std::map<int,std::string> > valuemap;
static int records;

static void
load_idpatch(void)
{
  FILE *f;

  f = fopen("id-patch.txt", "r");
  for(;;)
  {
    int id, patch;
    char buf[20], *pp;

    if(fgets(buf, 19, f) == NULL)
      break;

    if(strlen(buf) <= 4 || buf[0] == '#' || (pp = strchr(buf, ' ')) == NULL)
      continue;

    id = strtol(buf, NULL, 10);
    patch = strtol(pp, NULL, 10);

    idpatch[id] = patch;
  }
  fclose(f);
}

static void
load_valuemap(void)
{
  FILE *f;
  std::string mapname;
  char *p, *q;

  /* The map file is based on the php-website id->name mapping,
   * hence the MAP[foo] = array(bar) notation
   */
  f = fopen(OUTPUT_XML_MAP_FILENAME, "r");
  if(f == NULL)
  {
    fprintf(stderr, "Unable to open %s for reading.\n", OUTPUT_XML_MAP_FILENAME);
    exit(1);
  }
  for(;;)
  {
    char line[0xff];

    if(fgets(line, 0xfe, f) == NULL)
      break;

    if(mapname.empty())
    {
      if((p = strstr(line, "$GLOBALS")) != NULL)
      {
        if((p = strstr(p, "MAP")) != NULL)
        {
          if((p = strchr(p, '[')) != NULL)
          {
            while(*p != '\'' && *p != '"')
              p++;
            *strchr(p+1, *p) = 0;
            mapname = p+1;
          }
        }
      }
    }
    else
    {
      int key;
      std::string value;

      p = line;
      while(isspace(*p) && *p)
        p++;
      if(!*p)
        continue;

      key = strtol(p, NULL, 0);

      if((p = strchr(line, '"')) == NULL)
        continue; // somethings fucked up
      if((q = strchr(++p, '"')) == NULL)
        continue; // field never ended?
      *q++ = 0;

      value = p;

      //printf("Type: %s, Key: %d, Value: %s\n", mapname.c_str(), key, value.c_str());

      valuemap[mapname][key] = value;

      if(strstr(q, ");") != NULL)
      {
        mapname = "";
      }
    }
  }
  fclose(f);
}

static void
load_maps(config_vars_t *)
{
  load_idpatch();
  load_valuemap();

  printf("<?xml version=\"1.0\"?>\n\n");
  printf("<aodb>\n\n");
}

static void
close_xml(void)
{
  printf("\n</aodb>\n\n");
}

static int
flush_xml(void)
{
  int x = records;
  records = 0;
  return x;
}

static char *
escape_xml(char *from)
{
  static char b[0x2000];
  char *d = b, *s = from;
  size_t len, i;

  if(from == NULL)
  {
    *b = 0;
    return b;
  }

  for(i=0, len=strlen(from); i<len; i++)
  {
    switch(*s)
    {
      case '&' :
        strcpy(d, "&amp;");
        d += 4;
        break;

      case '<' :
        strcpy(d, "&lt;");
        d += 3;
        break;

      case '>' :
        strcpy(d, "&gt;");
        d += 3;
        break;

      case '\r' :
      case '\n' :
        *d = *s;
        break;

      default :
        if((unsigned char)(*s) <= 0x1f || (unsigned char)(*s) >= 0x80)
        {
          sprintf(d, "&#%03d;", (unsigned char)(*s));
          d += 5;
        }
        else
        {
          *d = *s;
        }
    }
    s++;
    d++;
  }
  *d = 0;
  return b;
}

static char *
map_id_word(int x)
{
  static char buf[5];
  sprintf(buf, "%4.4s", (char*)&x);
  return buf;
}

static const char *
map_value(std::string type, int key)
{
  std::map<int,std::string>::const_iterator i = valuemap[type].find(key);
  if(i != valuemap[type].end())
    return i->second.c_str();

  static char str[0xff];
  sprintf(str, "%d", key);
  return str;
}

static const char *
map_value_bits(std::string type, int bits, bool need_shift)
{
  static char str[0x400];
  std::map<int,std::string>::iterator mpi;

  *str = 0;

  for(mpi=valuemap[type].begin(); mpi!=valuemap[type].end(); mpi++)
  {
    if((need_shift ? (1<<mpi->first) : mpi->first) & bits)
    {
      strcat(str, mpi->second.c_str());
      strcat(str, ", ");
    }
  }

  if(*str != 0)
    *(strrchr(str, 0)-2) = 0;

  return str;
}

static bool
describe_effect(eff_descr_t *desc, eff_t *eff)
{
  desc->target = map_value("effect-target", eff->target);
  switch(eff->type)
  {
    case 0x0a :
      desc->attribute << map_value("skill", eff->values[0]);
      if(desc->attribute.str() == "maps a" || desc->attribute.str() == "maps b")
      {
        desc->action << "Modify";
        desc->value << map_value(desc->attribute.str(), eff->values[1]);
      }
      else
      {
        desc->action << "Hit";
        desc->value << eff->values[1] << " .. " << eff->values[2];
        if(eff->values[3] > 0)
          desc->value << " " << map_value("skill", eff->values[3]);
      }
      break;

    case 0x14 :
      desc->action << "Modify";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1];
      break;

    case 0x16 :
      desc->action << "Temporary";
      desc->attribute << map_value("skill", eff->values[0]) << " for " << (eff->values[4]/100.0) << "s";
      desc->value << eff->values[1];
      break;

    case 0x18 :
      desc->action << "Teleport";
      if(eff->values[3] == 0)
        desc->attribute << "current playfield";
      else
        desc->attribute << map_value("playfields", eff->values[3]);
      desc->value << "X: " << eff->values[0] << ", Y: " << eff->values[2] << ", Z: " << eff->values[1];
      break;

    case 0x1b :
      desc->action << "Upload";
      desc->attribute << "Nano";
      desc->value << eff->values[0];
      break;

    case 0x22 :
      desc->action << "Set";
      desc->attribute << map_value("skill", eff->values[1]);
      desc->value << eff->values[2];
      break;

    case 0x24 : // shadowlands
      desc->action << "Add Skill";
      desc->attribute << map_value("skill", eff->values[2]);
      break;

    case 0x26 : // XXX some sort of graphics effect
      desc->action << "Graphics";
      desc->attribute << "Effect";
      desc->value << eff->values[0];
      break;

    case 0x28 :
      desc->action << "Save";
      desc->attribute << "Character";
      break;

    case 0x29 :
      desc->action << "Lock";
      desc->attribute << map_value("skill", eff->values[1]);
      desc->value << eff->values[2] << "s";
      break;

    case 0x2b :
      desc->action << "Mesh";
      desc->attribute << "Head";
      desc->value << eff->values[1];
      break;

    case 0x2d :
      desc->action << "Mesh";
      desc->attribute << "Back";
      desc->value << eff->values[1];
      break;

    case 0x2f :
      desc->action << "Texture";
      desc->attribute << map_value("textures", eff->values[1]);
      desc->value << eff->values[0];
      break;

    case 0x34 :
      desc->action << "Text";
      desc->attribute << "Chat";
      desc->value << eff->values[0];
      break;

    case 0x35 :
      desc->action << "Modify";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1];
      break;

    case 0x3b :
      desc->action << "Cast";
      desc->attribute << "Nano";
      desc->value << eff->values[0];
      break;

    case 0x3e :
      desc->action << "Mesh";
      desc->attribute << "Body";
      desc->value << eff->values[0];
      break;

    case 0x3f :
      desc->action << "Mesh";
      desc->attribute << "Attractor";
      desc->value << eff->values[1];
      break;

    case 0x41 :
      desc->action << "Text";
      desc->attribute << "Float";
      desc->value << eff->values[0];
      break;

    case 0x44 :
      desc->action << "Change";
      desc->attribute << "Shape";
      desc->value << eff->values[0];
      break;

    case 0x47 :
      desc->action << "Summon";
      desc->attribute << "Monster";
      desc->value << map_id_word(eff->values[0]);
      break;

    case 0x48 :
      desc->action << "Summon";
      desc->attribute << "Item";
      desc->value << map_id_word(eff->values[0]);
      break;

    case 0x4a :
      desc->action << "Cast";
      desc->attribute << "Nano";
      desc->value << eff->values[0];
      desc->target = "team";
      break;

    case 0x4b :
      desc->action << "Status";
      desc->attribute << map_value_bits("status", eff->values[0], true);
      desc->value << eff->values[1];
      break;

    case 0x4c :
      desc->action << "Restrict";
      desc->attribute << "Action";
      desc->value << map_value_bits("status", eff->values[0], true);
      break;

    case 0x4d :
      desc->action << "Model";
      desc->attribute << "Head";
      desc->value << "Next";
      break;

    case 0x4e :
      desc->action << "Model";
      desc->attribute << "Head";
      desc->value << "Previous";
      break;

    case 0x51 :
      desc->action << "AOE " << eff->values[4] << "m";
      desc->attribute << map_value("skill", eff->values[0]);
      if(eff->values[3] > 0)
        desc->value << map_value("skill", eff->values[3]) << " ";
      desc->value << eff->values[1] << " .. " << eff->values[2];
      break;

    case 0x57 :
      desc->action << "Change";
      desc->attribute << "Vision";
      desc->value << eff->values[0];
      break;

    case 0x5b :
      desc->action << "Teleport";
      desc->value << eff->values[0] << " " << eff->values[1] << " " << eff->values[4];
      break;

    case 0x5e :
      desc->action << "Reload";
      desc->attribute << "Model";
      break;

    case 0x5f :
      desc->action << "AOE " << eff->values[1] << "m";
      desc->attribute << "Nano";
      desc->value << eff->values[0];
      break;

    case 0x61 :
      desc->action << "Cast";
      desc->attribute << "Nano with " << eff->values[1] << "% chance";
      desc->value << eff->values[0];
      break;

    case 0x64 :
      desc->action << "Open";
      desc->attribute << "Bank";
      break;

    case 0x6c :
      desc->action << "Equip";
      desc->attribute << "Weapon";
      desc->value << map_id_word(eff->values[0]);
      break;

    case 0x71 :
      desc->action << "Remove";
      desc->attribute << map_value("school", eff->values[1]) << "nanos &lt;= " << eff->values[0] << " NCU";
      desc->value << eff->values[2] << " times";
      break;

    case 0x73 :
      desc->action << "Script";
      desc->value << eff->values[0];
      break;

    case 0x75 : // create apartment or enter apartment
      desc->action << "Enter";
      desc->attribute << "Apartment";
      break;

    case 0x76 :
      desc->action << "Change";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1];
      break;

    case 0x7b :
      desc->action << "Text";
      desc->attribute << "Input";
      desc->value << eff->values[0];
      break;

    case 0x7d :
      desc->action << "Taunt";
      desc->value << eff->values[2];
      break;

    case 0x7e : // pacify
      desc->action << "Pacify";
      break;

    case 0x81 : // fear
      desc->action << "Fear";
      break;

    case 0x84 :
      desc->action << "Summon";
      desc->attribute << "item with " << eff->values[0] << "% chance";
      desc->value << map_id_word(eff->values[1]);
      break;

    case 0x86 : // wipe hate list
      desc->action << "Wipe";
      desc->attribute << "Hate list";
      break;

    case 0x87 : // charm
      desc->action << "Charm";
      break;

    case 0x88 : // daze
      desc->action << "Daze";
      break;

    case 0x8a : // destroy item
      desc->action << "Destroy";
      desc->attribute << "Item";
      break;

    case 0x8c :
      desc->action << "Text";
      desc->attribute << "Name";
      desc->value << eff->values[0];
      break;

    case 0x8d : // set organization type
      desc->action << "Set";
      desc->attribute << "Organization type";
      desc->value << map_value(desc->attribute.str(), eff->values[0]);
      break;

    case 0x8e :
      desc->action << "Text";
      desc->value << eff->values[0];
      break;

    case 0x91 : // create apartment or enter apartment
      desc->action << "Create";
      desc->attribute << "Apartment";
      break;

    case 0x92 : // zero content, enable flight
      desc->action << "Enable";
      desc->attribute << "Flight";
      break;

    case 0x93 : // set flag
      desc->action << "Set Flag";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << map_value(desc->attribute.str(), 1<<eff->values[1]);
      break;

    case 0x94 : // name change?
      desc->action << "Enable";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1];
      break;

    case 0x98 : // warp to last save
      desc->action << "Warp";
      desc->attribute << "Last save point";
      break;

    case 0xa2 : // summon selected player
      desc->action << "Summon";
      desc->attribute << "Selected player";
      break;

    case 0xa3 : // summon team members
      desc->action << "Summon";
      desc->attribute << "Team members";
      break;

    case 0xaa :
      desc->action << "Resistance";
      desc->attribute << "Nano strain " << eff->values[0];
      desc->value << eff->values[1] << "%";
      break;

    case 0xac :
      desc->action << "Save";
      desc->attribute << "SL character";
      break;

    case 0xaf :
      desc->action << "Spawn";
      desc->attribute << "Pet";
      desc->value << map_id_word(eff->values[0]);
      break;

    case 0xb7 :
      desc->action << "Advantage";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1];
      break;

    case 0xb9 :
      desc->action << "Reduce";
      desc->attribute << "Nano strain " << eff->values[0];
      desc->value << eff->values[1] << "s";
      break;

    case 0xba : // shield disabled
      desc->action << "Disable";
      desc->target = "LC area";
      desc->attribute << "Defense shield";
      break;

    case 0xbd : // pet warper
      desc->action << "Warp";
      desc->attribute << "Pets";
      break;

    case 0xbe : // shadowlands stuff - add action
      desc->action << "Add Action";
      desc->attribute << eff->values[3];
      desc->value << map_id_word(eff->values[1]);
      break;

    case 0xc0 : // shadowlands stuff - modify by %
      desc->action << "Modify";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1] << "%";
      break;

    case 0xc1 : // shadowlands stuff - drain hit
      desc->action << "Drain Hit";
      desc->attribute << map_value("skill", eff->values[0]) << " recover " << eff->values[4] << "%";
      desc->value << eff->values[1] << " .. " << eff->values[2];
      if(eff->values[3] > 0)
        desc->value << " " << map_value("skill", eff->values[3]);
      break;

    case 0xc3 : // shadowlands stuff - lock perk
      desc->action << "Lock Perk";
      desc->attribute << eff->values[1];
      desc->value << eff->values[2] << "s";
      break;

    case 0xcc : // shadowlands stuff - special hit
      desc->action << "Special Hit";
      desc->attribute << map_value("skill", eff->values[0]);
      desc->value << eff->values[1] << " .. " << eff->values[2];
      if(eff->values[3] > 0)
        desc->value << " " << map_value("skill", eff->values[3]);
      break;

    case 0xd8 : // shadowlands stuff - set anchor
      desc->action << "Set Anchor";
      break;

    case 0xd9 : // shadowlands stuff - recall to anchor
      desc->action << "Recall Anchor";
      break;

    default :
      return false;
  }

  return true;
}

static void
print_requirement_table(std::list<req_t> reqs, short indent)
{
  if(reqs.empty())
    return;

  printf("%*s<requirements>\n", indent, "");

  std::list<req_t>::iterator ti;
  for(ti=reqs.begin(); ti!=reqs.end(); ti++)
  {
    int count = ti->count;
    const char *op;

    switch(ti->op)
    {
      case 0 :
        op = "exactly";
        break;

      case 1 :
        op = "no more than";
        count --;
        break;

      case 2 :
        op = "at least";
        count ++;
        break;

      case 24 :
        op = "other than";
        break;

      default :
        op = "unknown";
    }

    printf("%*s<requirement hook=\"%s\" attribute=\"%s\" operator=\"%s\" value=\"%d\" />\n",
      indent+2, "",
      map_value("requirement-hook", ti->type),
      map_value("skill", ti->attribute),
      op,
      count);
  }

  printf("%*s</requirements>\n", indent, "");
}

static void
print_effect_table(std::list<eff_t> effs, short indent)
{
  if(effs.empty())
    return;

  printf("%*s<effects>\n", indent, "");

  std::list<eff_t>::iterator ti;
  for(ti=effs.begin(); ti!=effs.end(); ti++)
  {
    eff_descr_t edesc;
    if(describe_effect(&edesc, &*ti) == false)
      continue;

    printf("%*s<effect hook=\"%s\" type=\"%d\" target=\"%s\"",
      indent+2, "",
      map_value("effect-hook", ti->hook),
      ti->type, edesc.target.c_str());

    if(ti->hits > 1)
      printf(" hits=\"%d\" delay=\"%d\"", ti->hits, ti->delay);
    if(edesc.action.str().length())
      printf(" action=\"%s\"", edesc.action.str().c_str());
    if(edesc.attribute.str().length())
      printf(" attribute=\"%s\"", edesc.attribute.str().c_str());
    if(edesc.value.str().length())
      printf(" value=\"%s\"", edesc.value.str().c_str());

    if(ti->reqs.empty() && ti->text == NULL)
    {
      printf(" />\n");
    }
    else
    {
      printf(">\n");
      if(ti->text)
        printf("%*s<text>%s</text>\n", indent+4, "", escape_xml(ti->text));
      if(ti->reqs.size())
        print_requirement_table(ti->reqs, indent+4);
      printf("%*s</effect>\n", indent+2, "");
    }
  }

  printf("%*s</effects>\n", indent, "");
}

static void
print_skill_table(std::map<int,int> skills, short indent, const char *name)
{
  if(skills.empty())
    return;

  printf("%*s<skillmap type=\"%s\">\n", indent, "", name);

  std::map<int,int>::iterator ti;
  for(ti=skills.begin(); ti!=skills.end(); ti++)
  {
    printf("%*s<skill name=\"%s\" percentage=\"%d\" />\n",
      indent+2, "", map_value("skill", ti->first), ti->second);
  }

  printf("%*s</skillmap>\n", indent, "");
}

static void
store_item(void *itemp)
{
  item_t *item = (item_t*)itemp;
  std::map<int,int>::const_iterator idpi = idpatch.find(item->aoid);
  if(idpi != idpatch.end())
    item->patch = idpi->second;
  else
    item->patch = 110000;

  records ++;

  printf("  <item aoid=\"%d\" patch=\"%d\" metatype=\"%c\">\n", item->aoid, item->patch, item->metatype);
  printf("    <name>%s</name>\n",  escape_xml(item->name));
  printf("    <description>%s</description>\n", escape_xml(item->info));
  printf("    <bitfield type=\"flags\">%s</bitfield>\n", map_value_bits("bits-flags", item->flags, true));
  printf("    <bitfield type=\"props\">%s</bitfield>\n", map_value_bits("bits-props", item->props, true));
  printf("    <ql>%d</ql>\n", item->ql);
  if(item->metatype != 'n') /* nanos dont have type, slot or value */
  {
    printf("    <type>%d</type>\n", item->type);
    if(item->slot > 0)
    {
      char buf[9];
      sprintf(buf, "slots-%d", item->type);
      printf("    <slots default=\"%s\">%s</slots>\n",
        map_value(buf, 1<<item->defslot), map_value_bits(buf, item->slot, false));
    }
    printf("    <value>%d</value>\n", item->value);
  }
  printf("    <icon>%d</icon>\n", item->icon);
  printf("    <times equip=\"%d\" attack=\"%d\" recharge=\"%d\" />\n", item->tequip, item->tattack, item->trecharge);
  if(item->dmin || item->dmax || item->dcrit || item->dtype)
    printf("    <damage minimum=\"%d\" maximum=\"%d\" critical=\"%d\" type=\"%d\" />\n", item->dmin, item->dmax, item->dcrit, item->dtype);
  if(item->atype || item->clip)
    printf("    <ammo type=\"%s\" clipsize=\"%d\" />\n", map_value("ammo", item->atype), item->clip);
  if(item->range)
    printf("    <range range=\"%d\" />\n", item->range);
  if(item->crystal || item->nanocost || item->ncu)
    printf("    <nanodata crystalid=\"%d\" nanocost=\"%d\" ncu=\"%d\" />\n", item->crystal, item->nanocost, item->ncu);
  if(item->school || item->strain)
    printf("    <nanoclass school=\"%s\" strain=\"%d\" />\n", map_value("school", item->school), item->strain);
  if(item->duration)
    printf("    <duration duration=\"%d\" />\n", item->duration);

  print_requirement_table(item->reqs, 4);
  print_effect_table(item->effs, 4);
  print_skill_table(item->attmap, 4, "attack");
  print_skill_table(item->defmap, 4, "defense");

  if(!item->other.empty())
  {
    printf("    <other>\n");

    std::map<int,int>::iterator ti;
    for(ti=item->other.begin(); ti!=item->other.end(); ti++)
    {
      printf("      <attribute key=\"%s\" value=\"%d\" />\n", map_value("skill", ti->first), ti->second);
    }

    printf("    </other>\n");
  }

  printf("  </item>\n\n");
}

aoppa_plugin_t aoppa_plugin =
{
  AOPPA_API_VERSION,	// api version
  AOPPA_OUTPUT,		// type
  load_maps,		// init
  close_xml,		// finish
  store_item,		// store
  flush_xml,		// flush
  NULL,			// parser
  "XML output plugin"	// name
};
