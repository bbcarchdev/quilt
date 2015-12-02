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

/* Add the details of req to a 'request' member of the dictionary */
int
html_add_request(json_t *dict, QUILTREQ *req)
{
	json_t *r, *a;
	char *pathbuf, *t;
	QUILTTYPE typebuf, *type;
	size_t l;
	const char *s, *path, *reqtype;

	r = json_object();
	pathbuf = NULL;
	path = quilt_request_path(req);
	reqtype = quilt_request_type(req);
	if(path)
	{
		pathbuf = (char *) malloc(strlen(path) + 32);
		json_object_set_new(r, "path", json_string(path));
		if(quilt_request_home(req))
		{
			strcpy(pathbuf, "/index");
		}
		else
		{
			strcpy(pathbuf, path);
		}
		json_object_set_new(r, "document", json_string(pathbuf));
	}
#define GETPROP(r, req, name, accessor)	\
	if((s = accessor(req))) \
	{ \
		json_object_set_new(r, name, json_string(s)); \
	}
	GETPROP(r, req, "ext", quilt_request_ext);
	GETPROP(r, req, "type", quilt_request_type);
	GETPROP(r, req, "host", quilt_request_host);
	GETPROP(r, req, "ident", quilt_request_ident);
	GETPROP(r, req, "user", quilt_request_user);
	GETPROP(r, req, "method", quilt_request_method);
	GETPROP(r, req, "referer", quilt_request_referer);
	GETPROP(r, req, "ua", quilt_request_ua);
	json_object_set_new(r, "status", json_integer(quilt_request_status(req)));
	GETPROP(r, req, "statustitle", quilt_request_statustitle);
	GETPROP(r, req, "statusdesc", quilt_request_statusdesc);
	json_object_set_new(dict, "request", r);
	json_object_set_new(dict, "home", (quilt_request_home(req) ? json_true() : json_false()));
	json_object_set_new(dict, "index", (quilt_request_index(req) ? json_true() : json_false()));
	GETPROP(dict, req, "title", quilt_request_indextitle);
#undef GETPROP
	if(pathbuf)
	{
		a = json_array();
		t = strchr(pathbuf, 0);
		*t = '.';
		t++;
		for(type = quilt_plugin_serializer_first(&typebuf); type; type = quilt_plugin_next(type))
		{
			if(!type->visible || !type->extensions)
			{
				continue;
			}
			if(reqtype && !strcasecmp(reqtype, type->mimetype))
			{
				continue;
			}
			s = strchr(type->extensions, ' ');
			if(s)
			{
				l = s - type->extensions;			   
			}
			else
			{
				l = strlen(type->extensions);
			}
			if(l > 6)
			{
				continue;
			}			
			strncpy(t, type->extensions, l);
			t[l] = 0;
			r = json_object();
			json_object_set_new(r, "type", json_string(type->mimetype));
			if(type->desc)
			{
				json_object_set_new(r, "title", json_string(type->desc));
			}
			json_object_set_new(r, "uri", json_string(pathbuf));
			json_object_set_new(r, "ext", json_string(t));
			json_array_append_new(a, r);
			quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": linking to %s as %s (%s)\n", pathbuf, type->mimetype, type->desc);
		}
		json_object_set_new(dict, "links", a);
		free(pathbuf);
	}
	return 0;
}
