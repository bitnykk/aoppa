/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Program initialization.
 *
 * $Id: main.cc 1111 2009-03-04 07:57:14Z os $
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

#include <cctype>
#include <cstdio>
#include <cstring>

#include <dlfcn.h>
#include <unistd.h>

#include "aoppa.h"
#include "misc.h"
#include "parser.h"

typedef struct input_file_s
{
  const char *fn;
  int type;
  int patch;
} aofile_t;

static void show_help(const char *);
static aoppa_plugin_t *load_plugin(char *, char *);
static void parse_options(char *options, config_vars_t *);

int
main(int argc, char **argv)
{
  char *options = NULL, *plugin_name = NULL;
  std::list<aoppa_plugin_t*> plugins;
  std::list<aoppa_plugin_t*>::iterator plugini;

  g_debug = 1; // lets have some output by default .. -q turns it off

  /* Parse argument options */
  int idx;
  while((idx = getopt(argc, (char*const*)argv, "vdqp:o:")) != -1)
  {
    switch(idx)
    {
      case 'v' :
        fprintf(stderr,
          "AOPPA version %s.  API version %d.\n"
          "\n"
          "Copyright (C) 2002-2009 Oskari Saarenmaa <auno@auno.org>.\n"
          "All rights reserved.\n"
          "\n"
          "Licensed under the GNU General Public License version 3+.\n",
            AOPPA_VERSION,
            AOPPA_API_VERSION);
        exit(0);
        break;

      case 'd' :
        g_debug ++;
        break;

      case 'q' :
        g_debug --;
        break;

      case 'p' :
        if(plugin_name != NULL)
        {
          plugins.push_back(load_plugin(plugin_name, options));
          options = plugin_name = NULL;
        }
        if(strchr(optarg, '/') == NULL)
        {
          plugin_name = (char*)malloc(strlen(optarg)+3);
          sprintf((char*)plugin_name, "./%s", optarg);
        }
        else
        {
          plugin_name = strdup(optarg);
        }
        break;

      case 'o' :
        options = strdup(optarg);
        break;

    }
  }
  if(plugin_name != NULL)
    plugins.push_back(load_plugin(plugin_name, options));

  if(plugins.empty())
  {
    debugf(0, "No plugins have been defined.  Nothing to do.\n");
    exit(1);
  }

  /* Check input files */
  std::list<aofile_t> files;
  while(optind < argc)
  {
    char *p, *filename = strdup((const char*)(argv[optind++]));
    int patch = 0, filetype = 0;

    if(access(filename, R_OK))
    {
      debugf(0, "File '%s' does not exist or is not readable, skipping\n", filename);
      free(filename);
      continue;
    }

    /* This is a bit stupid, it should be left for the input plugin to figure out
     * what kind of data an input file contains, and to decide if the file should
     * be used or discarded.
     */
    if((p = strstr(filename, "nano")) != NULL)
    {
      filetype = AOPT_NANO;
    }
    else if((p = strstr(filename, "item")) != NULL)
    {
      filetype = AOPT_ITEM;
    }
    else
    {
      debugf(0, "File '%s' is neither item or nano data, skipping\n", filename);
      free(filename);
      continue;
    }

    while(*p != 0 && !isdigit(*p))
      p++;
    if(*p == 0)
    {
      debugf(0, "Could not find a sequence of numbers in the filename '%s', skipping\n", filename);
      free(filename);
      continue;
    }

    patch = atoi(p);
    files.push_back((aofile_t){filename, filetype, patch});
  }

  if(files.empty())
  {
    debugf(0, "No input files specified.\n");
    show_help((const char *)(argv[0]));
    exit(1);
  }

  /* Initialize plugin function pointer arrays */
  std::vector<plugin_store_t*> store;
  std::vector<plugin_flush_t*> flush;
  std::vector<plugin_parse_t*> parse;
  for(plugini=plugins.begin(); plugini!=plugins.end(); plugini++)
  {
    if((*plugini)->store != NULL)
      store.push_back((*plugini)->store);
    if((*plugini)->flush != NULL)
      flush.push_back((*plugini)->flush);
    if((*plugini)->parse != NULL)
      parse.push_back((*plugini)->parse);
  }

  /* Initialize parser */
  debugf(3, "Initializing parser\n");
  init_parser(store);

  /* Process the files */
  std::list<aofile_t>::iterator fi;
  debugf(1, "Going to process %d files...\n", files.size());
  if(g_debug >= 3)
  {
    for(fi=files.begin(); fi!=files.end(); fi++)
      debugf(3, "--> '%s', patch: %d, %s\n", fi->fn, fi->patch, (fi->type == AOPT_ITEM) ? "items" : "nanos");
  }

  for(fi=files.begin(); fi!=files.end(); fi++)
  {
    debugf(3, "Parsing file %s (%d, %d)\n", fi->fn, fi->patch, fi->type);
    for(std::vector<plugin_parse_t*>::iterator ppi=parse.begin(); ppi!=parse.end(); ppi++)
      (**ppi)(fi->patch, fi->type, fi->fn);
    for(std::vector<plugin_flush_t*>::iterator pfi=flush.begin(); pfi!=flush.end(); pfi++)
      (**pfi)();
  }

  /* Finish and clean up */
  debugf(3, "Destroying parser\n");
  destroy_parser();

  store.clear();
  flush.clear();
  parse.clear();

  for(plugini=plugins.begin(); plugini!=plugins.end(); plugini++)
    if((*plugini)->finish != NULL)
      (*(*plugini)->finish)();

  debugf(2, "All done\n");

  return 0;
}

static void
parse_options(char *options, config_vars_t *cfg)
{
  if(options != NULL)
  {
    char *p = (char*)options, *key, *val;
    debugf(3, "Parsing plugin options.\n");
    while(p != NULL && *p)
    {
      key = p;
      if((val = strchr(key, '=')) == NULL)
        break;
      *val++ = 0;
      if((p = strchr(val, ',')) != NULL)
        *p++ = 0;

      debugf(3, "Got option '%s' == '%s'\n", key, val);

      (*cfg)[key] = val;
    }
  }
}

static aoppa_plugin_t *
load_plugin(char *plugin_name, char *options)
{
  const char *errstr;
  void *plugin;
  aoppa_plugin_t *plugin_info;
  config_vars_t cfg;
  parse_options(options, &cfg);

  /* Load */
  plugin = dlopen(plugin_name, RTLD_NOW);
  if((errstr = dlerror()) != NULL)
  {
    debugf(0, "%s\n", errstr);
    exit(1);
  }

  /* Find our data symbol */
  plugin_info = (aoppa_plugin_t*)dlsym(plugin, "aoppa_plugin");
  if((errstr = dlerror()) != NULL)
  {
    debugf(0, "%s\n", errstr);
    exit(1);
  }

  /* Check versions */
  if(plugin_info->aoppa_api_version != AOPPA_API_VERSION)
  {
    debugf(0, "%s: version differs from AOPPA version. [%d vs %d]\n",
      plugin_name, plugin_info->aoppa_api_version, AOPPA_API_VERSION);
    exit(1);
  }

  /***
   *** no need for this
   ***
  if((plugin_info->aoppa_type == 0) ||
     (plugin_info->aoppa_type & (plugin_info->aoppa_type)-1))
  {
    debugf(0, "%s: type is either empty or has multiple bits set. [0x%08x]\n",
      plugin_name, plugin_info->aoppa_type);
    exit(1);
  }
   ***
   ***/

  /* Initialize plugin */
  if(plugin_info->init != NULL)
    (*plugin_info->init)(&cfg);

  if(options != NULL)
    free(options);
  if(plugin_name != NULL)
    free(plugin_name);

  return plugin_info;
}

const char *
aoppa_get_config(config_vars_t *cfg, const char *val)
{
  config_vars_t::const_iterator cfgkey = cfg->find(val);
  if(cfgkey == cfg->end())
    return NULL;
  else
    return cfgkey->second.c_str();
}

static void
show_help(const char *fn)
{
  debugf(0, "Usage: %s -p <plugin> [flags] <files>\n", fn);
  debugf(0, "  Where the flags are:\n");
  debugf(0, "          -p x  - use plugin x for data storage. mandatory\n");
  debugf(0, "          -o x  - options to pass to the plugin, in format:\n");
  debugf(0, "                  key1=val1,key2=val2,...\n");
  debugf(0, "          -d    - raise debug level\n");
  debugf(0, "          -q    - lower debug level\n");
  debugf(0, "  Any non-option arguments are treated as input data files\n");
}
