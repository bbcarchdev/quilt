/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

static char *baseuri;
static size_t baseurilen;
static char *templatedir;
static size_t templatedirlen;

static LIQUIFY *liquify;

static LIQUIFYTPL *tpl_home;
static LIQUIFYTPL *tpl_index;
static LIQUIFYTPL *tpl_item;
static LIQUIFYTPL *tpl_error;

static int html_serialize(QUILTREQ *req);
static LIQUIFYTPL *quilt_html_parse_(LIQUIFY *liquify, const char *pathname, void *data);
static LIQUIFYTPL *quilt_html_template_(QUILTREQ *req);
static int add_req(json_t *dict, QUILTREQ *req);
static int add_data(json_t *dict, QUILTREQ *req);
static int add_subject(QUILTREQ *req, json_t *item, librdf_model *model, librdf_node *subject, const char *uri);
static int add_predicate(QUILTREQ *req, json_t *value, librdf_node *predicate, const char *uri);
static int add_object(QUILTREQ *req, json_t *value, librdf_node *object);
static char *get_title(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_shortdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_longdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject);
static char *get_literal(QUILTREQ *req, librdf_model *model, librdf_node *subject, const char *predicate);

/* Quilt plug-in entry-point */
int
quilt_plugin_init(void)
{
	size_t c;

	baseuri = quilt_config_geta("quilt:base", NULL);
	if(!baseuri)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to determine base URI from configuration\n");
		return -1;
	}
	baseurilen = strlen(baseuri);
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
	liquify_set_loader(liquify, quilt_html_parse_, NULL);
	tpl_home = liquify_load(liquify, "home.liquid");
	tpl_item = liquify_load(liquify, "item.liquid");
	tpl_index = liquify_load(liquify, "index.liquid");
	tpl_error = liquify_load(liquify, "error.liquid");
	for(c = 0; html_types[c].mimetype; c++)
	{
		quilt_plugin_register_serializer(&(html_types[c]), html_serialize);
	}
	return 0;
}

static int
html_serialize(QUILTREQ *req)
{
	LIQUIFYTPL *tpl;
	json_t *dict;
	char *buf, *loc;
	int status;

	status = 500;
	dict = json_object();
	add_req(dict, req);
	add_data(dict, req);
	/*	json_dumpf(dict, stderr, JSON_INDENT(4)); 
		exit(0); */
	tpl = quilt_html_template_(req);
	if(tpl)
	{
		/* Set status to zero to suppress output */
		status = 0;
		buf = liquify_apply(tpl, dict);
		loc = quilt_canon_str(req->canonical, QCO_CONCRETE|QCO_NOABSOLUTE);
		quilt_request_printf(req, "Status: %d %s\n"
							 "Content-Type: %s; charset=utf-8\n"
							 "Content-Location: %s\n"
							 "Vary: Accept\n"
							 "Server: Quilt/" PACKAGE_VERSION "\n"
							 "\n", req->status, req->statustitle, req->type, loc);
		free(loc);
		quilt_request_puts(req, buf);
		free(buf);
	}
	json_decref(dict);
	return status;
}

static LIQUIFYTPL *
quilt_html_template_(QUILTREQ *req)
{
	if(req->status != 200)
	{
		return tpl_error;
	}
	if(req->home && tpl_home)
	{
		return tpl_home;
	}
	if((req->home || req->index) && tpl_index)
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

/* Add the details of req to a 'request' member of the dictionary */
static int
add_req(json_t *dict, QUILTREQ *req)
{
	json_t *r, *a;
	char *pathbuf, *t;
	QUILTTYPE typebuf, *type;
	size_t l;
	const char *s;

	r = json_object();
	pathbuf = NULL;
	if(req->path)
	{
		pathbuf = (char *) malloc(strlen(req->path) + 32);
		json_object_set_new(r, "path", json_string(req->path));
		if(req->path[0] == '/' && req->path[1] == 0)
		{
			strcpy(pathbuf, "/index");
		}
		else
		{
			strcpy(pathbuf, req->path);
		}
		json_object_set_new(r, "document", json_string(pathbuf));
	}
	if(req->ext) json_object_set_new(r, "ext", json_string(req->ext));
	if(req->type) json_object_set_new(r, "type", json_string(req->type));
	if(req->host) json_object_set_new(r, "host", json_string(req->host));
	if(req->ident) json_object_set_new(r, "ident", json_string(req->ident));
	if(req->user) json_object_set_new(r, "user", json_string(req->user));
	if(req->method) json_object_set_new(r, "method", json_string(req->method));
	if(req->referer) json_object_set_new(r, "referer", json_string(req->referer));
	if(req->ua) json_object_set_new(r, "ua", json_string(req->ua));

	json_object_set_new(r, "status", json_integer(req->status));
	if(req->statustitle)
	{
		json_object_set_new(r, "statustitle", json_string(req->statustitle));
	}
	if(req->errordesc)
	{
		json_object_set_new(r, "statusdesc", json_string(req->errordesc));
	}
	json_object_set_new(dict, "request", r);
	json_object_set_new(dict, "home", (req->home ? json_true() : json_false()));
	json_object_set_new(dict, "index", (req->index ? json_true() : json_false()));
	if(req->indextitle)
	{
		json_object_set_new(dict, "title", json_string(req->indextitle));
	}
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
			if(req->type && !strcasecmp(req->type, type->mimetype))
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

/* Add the data contained in the request's librdf model to a 'data'
 * member of the dictionary.
 */
static int
add_data(json_t *dict, QUILTREQ *req)
{
	json_t *items, *item, *k, *props, *prop, *value;
	librdf_world *world;
	librdf_statement *query, *statement;
	librdf_stream *st;
	librdf_node *subj, *pred, *obj;
	const char *uri;
	char *sbuf;

	world = quilt_librdf_world();
	/*
	  data[subject] = {
	     subject: 'http://...',
		 subjectLink: '<a href="...">...</a>',
		 classLabel: 'Thing',
		 classSuffix: '(Thing)',
		 props: {
		   '@': [
		     { predicate: '...', value: 'abc123', htmlValue: '<a href="...">abc123</a>', type: 'literal', datatype: null, language: null }
		   ]
		 }
     }
	*/
	items = json_object();
	query = librdf_new_statement(world);
	st = librdf_model_find_statements(req->model, query);
	while(!librdf_stream_end(st))
	{
		statement = librdf_stream_get_object(st);
		subj = librdf_statement_get_subject(statement);
		pred = librdf_statement_get_predicate(statement);
		obj = librdf_statement_get_object(statement);
		if(librdf_node_is_resource(subj) && librdf_node_is_resource(pred))
		{
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(subj));
			item = json_object_get(items, uri);
			if(item)
			{
				/* An entry for this subject URI already exists */
				props = json_object_get(item, "props");
			}
			else
			{
				/* Populate a new item structure */
				item = json_object();
				json_object_set_new(items, uri, item);
				json_object_set_new(item, "me", json_false());
				add_subject(req, item, req->model, subj, uri);
				props = json_object();			   
				json_object_set_new(item, "props", props);
			}

			/* Obtain the array of values for this predicate URI */
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(pred));
			prop = json_object_get(props, uri);
			if(!prop)
			{
				/* This is the first instance of this predicate that we've
				 * seen for this object; create a new array to hold the
				 * values */
				prop = json_array();
				json_object_set_new(props, uri, prop);
			}
			/* Add a new 'value' structure to the array of values (prop) */
			value = json_object();
			json_array_append_new(prop, value);
			add_predicate(req, value, pred, uri);
			add_object(req, value, obj);
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	librdf_free_statement(query);
	sbuf = (char *) calloc(1, strlen(req->subject) + 8);
	strcpy(sbuf, req->subject);
	strcat(sbuf, "#id");
	k = json_object_get(items, sbuf);
	if(k)
	{
		json_object_set_new(k, "me", json_true());
		json_object_set(dict, "object", k);
	}
	else if((k = json_object_get(items, req->subject)))
	{
		json_object_set_new(k, "me", json_true());
		json_object_set(dict, "object", k);
	}
	else if((k = json_object_get(items, req->path)))
	{
		json_object_set_new(k, "me", json_true());
		json_object_set(dict, "object", k);
	}
	free(sbuf);
	json_object_set_new(dict, "data", items);
	return 0;
}

/* Add the details of a specific subject to an 'item' structure which is
 * passed into the template.
 */
static int
add_subject(QUILTREQ *req, json_t *item, librdf_model *model, librdf_node *subject, const char *uri)
{
	double lon, lat;
	json_t *sub;
	struct class_struct *c;
	char *buf, *str;
	URI *uriobj;
	URI_INFO *info;

	uriobj = uri_create_str(uri, NULL);
	if(!uriobj)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to parse subject URI <%s>\n", uri);
		return -1;
	}
	info = uri_info(uriobj);

	json_object_set_new(item, "subject", json_string(uri));
	if(!strncmp(uri, baseuri, baseurilen))
	{
		buf = (char *) malloc(strlen(uri) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uri[baseurilen]));
		json_object_set_new(item, "link", json_string(buf));
		json_object_set_new(item, "uri", json_string(buf));
	}
	else
	{
		buf = quilt_uri_contract(uri);
		json_object_set_new(item, "link", json_string(uri));
		json_object_set_new(item, "uri", json_string(buf));
	}	
	c = html_class_match(model, subject);
	str = get_title(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "hasTitle", json_true());
		json_object_set_new(item, "title", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "hasTitle", json_false());
		json_object_set_new(item, "title", json_string(buf));
	}
	str = get_shortdesc(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "shortdesc", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "shortdesc", json_string(""));
	}
	str = get_longdesc(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "description", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "description", json_string(""));
	}
	if(buf[0] == '/' || !info->host)
	{
		json_object_set_new(item, "from", json_string(""));
	}
	else
	{
		strcpy(buf, "from ");
		strcpy(&(buf[5]), info->host);
		json_object_set_new(item, "from", json_string(buf));
	}		
	uri_info_destroy(info);
	free(buf);
	if(c)
	{
		json_object_set_new(item, "class", json_string(c->cssClass));
		json_object_set_new(item, "classLabel", json_string(c->label));
		json_object_set_new(item, "classSuffix", json_string(c->suffix));
		json_object_set_new(item, "classDefinite", json_string(c->definite));
	}
	else
	{
		json_object_set_new(item, "class", json_string(""));
		json_object_set_new(item, "classSuffix", json_string(""));
	}
	
	if(quilt_model_find_double(model, uri, "http://www.w3.org/2003/01/geo/wgs84_pos#long", &lon) == 1 &&
	   quilt_model_find_double(model, uri, "http://www.w3.org/2003/01/geo/wgs84_pos#lat", &lat) == 1)
	{
		sub = json_object();
		json_object_set_new(sub, "long", json_real(lon));
		json_object_set_new(sub, "lat", json_real(lat));
		json_object_set_new(item, "geo", sub);
	}
	return 0;
}

/* Add the details of a specific predicate to a 'value' structure which is
 * passed into the template.
 */
static int
add_predicate(QUILTREQ *req, json_t *value, librdf_node *predicate, const char *uri)
{
	char *contracted;

	(void) req;
	(void) predicate;

	json_object_set_new(value, "predicateUri", json_string(uri));
	contracted = quilt_uri_contract(uri);
	json_object_set_new(value, "predicateUriLabel", json_string(contracted));
	free(contracted);
	return 0;
}

/* Add the details of a triple's object to a 'value' structure which is
 * passed into the template
 */
static int
add_object(QUILTREQ *req, json_t *value, librdf_node *object)
{
	const char *str, *dtstr, *lang;
	char *buf;
	librdf_uri *dt;

	(void) req;

	if(librdf_node_is_resource(object))
	{
		str = (const char *) librdf_uri_as_string(librdf_node_get_uri(object));
		json_object_set_new(value, "type", json_string("uri"));
		json_object_set_new(value, "isUri", json_true());
		json_object_set_new(value, "value", json_string(str));
		if(!strncmp(str, baseuri, baseurilen))
		{
			buf = (char *) malloc(strlen(str) + 32);
			buf[0] = '/';
			strcpy(&(buf[1]), &(str[baseurilen]));
			json_object_set_new(value, "link", json_string(buf));
			json_object_set_new(value, "uri", json_string(buf));
		}
		else
		{
			buf = quilt_uri_contract(str);
			json_object_set_new(value, "uri", json_string(buf));
			json_object_set_new(value, "link", json_string(str));
		}
		free(buf);
	}
	else if(librdf_node_is_literal(object))
	{
		str = (const char *) librdf_node_get_literal_value(object);
		
		json_object_set_new(value, "type", json_string("literal"));
		json_object_set_new(value, "isLiteral", json_true());
		json_object_set_new(value, "value", json_string(str));

		lang = librdf_node_get_literal_value_language(object);
		if(lang)
		{
			json_object_set_new(value, "lang", json_string(lang));
		}
		dt = librdf_node_get_literal_value_datatype_uri(object);
		if(dt)
		{
			dtstr = (const char *) librdf_uri_as_string(dt);
			if(dtstr)
			{
				json_object_set_new(value, "datatype", json_string(dtstr));
				buf = quilt_uri_contract(dtstr);
				json_object_set_new(value, "datatypeUri", json_string(buf));
				free(buf);
			}
		}
		else
		{
			dtstr = NULL;
		}
	}
	return 0;
}

/* Obtain the title of a particular subject, suitable for substituting into
 * a template. This function should allocate the buffer it returns, or return
 * NULL if no suitable title could be found.
 */
static char *
get_title(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, "http://www.w3.org/2000/01/rdf-schema#label");
}

/* Obtain the short description of a particular subject, suitable for
 * substituting into a template. This function should allocate the buffer
 * it returns, or return NULL if no suitable title could be found.
 */
static char *
get_shortdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, "http://www.w3.org/2000/01/rdf-schema#comment");
}

/* Obtain the short description of a particular subject, suitable for
 * substituting into a template. This function should allocate the buffer
 * it returns, or return NULL if no suitable title could be found.
 */
static char *
get_longdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, "http://purl.org/dc/terms/description");
}

static char *
get_literal(QUILTREQ *req, librdf_model *model, librdf_node *subject, const char *predicate)
{
	librdf_world *world;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *obj;
	char *specific, *generic, *none;
	/* XXX perform proper language negotiation */
	const char *slang = "en-GB", *glang = "en";
	const char *l, *value;

	(void) req;
	
	world = quilt_librdf_world();
	specific = NULL;
	generic = NULL;
	none = NULL;
	obj = librdf_new_node_from_uri_string(world, (const unsigned char *) predicate);
	if(!obj)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create new URI for <%s>\n", predicate);
		return NULL;
	}
	query = librdf_new_statement(world);
	if(!query)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create new statement for query\n");
		librdf_free_node(obj);
		return NULL;
	}
	subject = librdf_new_node_from_node(subject);
	if(!subject)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to duplicate subject node\n");
		librdf_free_node(obj);
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, subject);
	librdf_statement_set_predicate(query, obj);
	stream = librdf_model_find_statements(model, query);
	if(!stream)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create stream for model query\n");
		librdf_free_statement(query);
		return NULL;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		obj = librdf_statement_get_object(st);
		if(!obj)
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to obtain object of statement\n");
		}
		else if(librdf_node_is_literal(obj) &&
		   !librdf_node_get_literal_value_datatype_uri(obj))
		{
			l = librdf_node_get_literal_value_language(obj);
			value = (const char *) librdf_node_get_literal_value(obj);
			if(!l)
			{
				if(!generic && !none)
				{
					none = strdup(value);
				}
			}
			else if(!strcasecmp(l, slang))
			{
				specific = strdup(value);
				break;
			}
			else if(!strcasecmp(l, glang) && !generic)
			{
				generic = strdup(value);
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(specific)
	{
		free(generic);
		free(none);
		return specific;
	}
	if(generic)
	{
		free(none);
		return generic;
	}
	return none;
}

/* Read the contents of a file into a new buffer */
static char *
readfile(const char *path)
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

/* Callback invoked by liquify_load() in order to load a template file
 * and parse it
 */
static LIQUIFYTPL *
quilt_html_parse_(LIQUIFY *liquify, const char *name, void *data)
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
	buf = readfile(pathname);
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
