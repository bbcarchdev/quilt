/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_html.h"

static char *html_readfile_(const char *path);
static LIQUIFYTPL *html_parse_(LIQUIFY *liquify, const char *name, void *data);

static char *templatedir;
static size_t templatedirlen;
static LIQUIFY *liquify;
static LIQUIFYTPL *tpl_home;
static LIQUIFYTPL *tpl_index;
static LIQUIFYTPL *tpl_item;
static LIQUIFYTPL *tpl_error;

/* Initialise the template engine and load templates */
int
html_template_init(void)
{
	templatedir = quilt_config_geta("html:templatedir", DATAROOTDIR "/" PACKAGE_TARNAME "/templates/");
	if(!templatedir)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to determine base path for templates\n");
		return -1;
	}
	templatedirlen = strlen(templatedir);
	liquify = liquify_create();
	if(!liquify)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to initialise templating context\n");
		return -1;
	}
	liquify_set_logger(liquify, quilt_vlogf);
	liquify_set_loader(liquify, html_parse_, NULL);
	tpl_home = liquify_load(liquify, "home.liquid");
	tpl_item = liquify_load(liquify, "item.liquid");
	tpl_index = liquify_load(liquify, "index.liquid");
	tpl_error = liquify_load(liquify, "error.liquid");	
	return 0;
}

/* Determine which template should be used for a request */
LIQUIFYTPL *
html_template(QUILTREQ *req)
{
	if(quilt_request_status(req) != 200)
	{
		return tpl_error;
	}
	if(quilt_request_home(req) && tpl_home)
	{
		return tpl_home;
	}
	if((quilt_request_home(req) || quilt_request_index(req)) && tpl_index)
	{
		return tpl_index;
	}
	if(tpl_item)
	{
		return tpl_item;
	}
	/* If all else fails, use the index or home templates (in order of
	 * preference.
	 */
	if(tpl_index)
	{
		return tpl_index;
	}
	return tpl_home;
}

/* Callback invoked by liquify_load() in order to load a template file
 * and parse it
 */
static LIQUIFYTPL *
html_parse_(LIQUIFY *liquify, const char *name, void *data)
{
	char *buf, *pathname;
	LIQUIFYTPL *template;   

	(void) data;

	pathname = (char *) malloc(strlen(name) + templatedirlen + 4);	
	if(!pathname)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate pathname buffer\n");
		return NULL;
	}
	if(name[0] == '/')
	{
		strcpy(pathname, name);
	}
	else
	{
		strcpy(pathname, templatedir);
		if(!templatedirlen || templatedir[templatedirlen - 1] != '/')
		{
			pathname[templatedirlen] = '/';
			pathname[templatedirlen + 1] = 0;
		}
		strcat(pathname, name);
	}
	buf = html_readfile_(pathname);
	if(!buf)
	{
		free(pathname);
		return NULL;
	}
	template = liquify_parse(liquify, name, buf, strlen(buf));
	free(pathname);
	free(buf);
	return template;
}

/* Read the contents of a file into a new buffer */
static char *
html_readfile_(const char *path)
{
	FILE *f;
	char *buf, *p;
	size_t len, alloc;
	ssize_t r;
	
	len = alloc = 0;
	buf = NULL;
	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": loading template: '%s'\n", path);
	f = fopen(path, "rb");
	if(!f)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": %s: (failed to open) %s\n", path, strerror(errno));
		return NULL;
	}
	while(1)
	{
		p = (char *) realloc(buf, alloc + 512);
		if(!p)
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": %s: %s\n", path, strerror(errno));
			free(buf);
			return NULL;
		}
		buf = p;
		alloc += 512;
		r = fread(&(buf[len]), 1, 511, f);
		if(r < 1)
		{
			break;
		}
		len += r;
	}
	buf[len] = 0;
	return buf;
}

