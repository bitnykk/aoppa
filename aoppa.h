/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Plugin datatype definitions.
 *
 * $Id: aoppa.h 1111 2009-03-04 07:57:14Z os $
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

#ifndef _AOPPA_AOPPA_H
#define _AOPPA_AOPPA_H

#include <map>
#include <list>
#include <string>
#include <vector>

#define AOPPA_VERSION		"0.5.5"
#define AOPPA_API_VERSION	2004021600

#define AOPPA_OUTPUT		0x0001
#define AOPPA_INPUT		0x0002

typedef std::map<std::string,std::string> config_vars_t;
typedef void plugin_init_t(config_vars_t *);
typedef void plugin_finish_t();
typedef void plugin_store_t(void *);
typedef int  plugin_flush_t();
typedef int  plugin_parse_t(int, int, const char *);

typedef struct aoppa_plugin_s
{
  unsigned int     aoppa_api_version;
  unsigned int     aoppa_type;

  plugin_init_t   *init;
  plugin_finish_t *finish;
  plugin_store_t  *store;
  plugin_flush_t  *flush;
  plugin_parse_t  *parse;

  const char      *comment;
} aoppa_plugin_t;

const char *aoppa_get_config(config_vars_t *, const char *);

/* Not used just yet.. */
#ifdef _AOPPA
#define DEBUG debugf
#else /* _AOPPA */
#define DEBUG ((void(*)(int,const char*,...))(aoppa_plugin.debugf))
#endif /* !_AOPPA */

#endif /* _AOPPA_AOPPA_H */
