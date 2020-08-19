/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Resourcedatabase parser datatype definitions and function prototypes.
 *
 * $Id: parser.h 1111 2009-03-04 07:57:14Z os $
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

#ifndef _AOPPA_PARSER_H
#define _AOPPA_PARSER_H

#define AOPT_ITEM		0x0000
#define AOPT_NANO		0x0001

#define AODB_ITEM_EFF_MAXVALS	0x0010

#include "aoppa.h"

void init_parser(std::vector<plugin_store_t*>);
void destroy_parser(void);
int  handle_item(int, int, int, size_t, unsigned char *);
void set_nano_crystal(int, int, int);
void set_item_hash(int, unsigned int);

typedef struct aodb_item_req_s
{
  int id;
  int type;
  int attribute;
  int count;
  int op;
  int opmod;
} req_t;

typedef struct aodb_item_eff_s
{
  int hook;
  int type;
  std::list<req_t> reqs;
  int hits;
  int delay;
  int target;
  int valuecount;
  int values[AODB_ITEM_EFF_MAXVALS];
  char *text;
} eff_t;

typedef struct aodb_item_s
{
  // aodb
  unsigned int hash;
  int  metatype;
  int  aoid;
  int  patch;
  int  flags;
  int  props;
  int  ql;
  int  icon;
  char *name;
  char *info;

  // aodb_item
  int type;
  int slot;
  int defslot;
  int value;
  int tequip;

  // aodb_nano
  int crystal;
  int ncu;
  int nanocost;
  int school;
  int strain;
  int duration;

  // aodb_weapon
  int multim;
  int multir;
  int tburst;
  int tfullauto;
  int clip;
  int dcrit;
  int atype;
  int mareq;

  // both
  int tattack;
  int trecharge;
  int dmax;
  int dmin;
  int range;
  int dtype;
  int initskill;
  std::map<int,int> attmap;
  std::map<int,int> defmap;

  // other tables
  std::map<int,int> other;
  std::list<req_t>  reqs;
  std::list<eff_t>  effs;
} item_t;

typedef struct aoid_ql_tuple
{
  int aoid;
  int ql;
} aoid_ql_t;

#endif
