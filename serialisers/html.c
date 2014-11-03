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

static int html_serialize(QUILTREQ *req);
static LIQUIFYTPL *quilt_html_parse_(LIQUIFY *liquify, const char *pathname, void *data);
static LIQUIFYTPL *quilt_html_template_(QUILTREQ *req);
static int add_req(jd_var *dict, QUILTREQ *req);
static int add_data(jd_var *dict, QUILTREQ *req);
static int add_subject(QUILTREQ *req, jd_var *item, librdf_model *model, librdf_node *subject, const char *uri);
static int add_predicate(QUILTREQ *req, jd_var *value, librdf_node *predicate, const char *uri);
static int add_object(QUILTREQ *req, jd_var *value, librdf_node *object);
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
		quilt_logf(LOG_CRIT, "failed to determine base URI from configuration\n");
		return -1;
	}
	baseurilen = strlen(baseuri);
	templatedir = quilt_config_geta("html:templatedir", DATAROOTDIR "/" PACKAGE_TARNAME "/templates/");
	if(!templatedir)
	{
		quilt_logf(LOG_CRIT, "failed to determine base path for templates\n");
		return -1;
	}
	templatedirlen = strlen(templatedir);
	liquify = liquify_create();
	if(!liquify)
	{
		quilt_logf(LOG_CRIT, "failed to initialise templating context\n");
		return -1;
	}
	liquify_set_logger(liquify, quilt_vlogf);
	liquify_set_loader(liquify, quilt_html_parse_, NULL);
	tpl_home = liquify_load(liquify, "home.liquid");
	tpl_item = liquify_load(liquify, "item.liquid");
	tpl_index = liquify_load(liquify, "index.liquid");
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
	jd_var *dict;
	char *buf;
	int status;

	status = 500;
	JD_SCOPE
	{		
		dict = jd_nhv(8);
		add_req(dict, req);
		add_data(dict, req);
		tpl = quilt_html_template_(req);
		if(tpl)
		{
			/* Set status to zero to suppress output */
			status = 0;
			buf = liquify_apply(tpl, dict);
			req->impl->printf(req, "Status: 200 OK\n"
						 "Content-type: %s; charset=utf-8\n"
						 "Vary: Accept\n"
						 "Server: Quilt/" PACKAGE_VERSION "\n"
						 "\n", req->type);
			req->impl->put(req, buf, strlen(buf));
			free(buf);
		}
	}
	return status;
}

static LIQUIFYTPL *
quilt_html_template_(QUILTREQ *req)
{
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
add_req(jd_var *dict, QUILTREQ *req)
{
	jd_var *r, *a;
	char *pathbuf, *t;

	r = jd_nhv(8);
	pathbuf = NULL;
	if(req->path)
	{
		pathbuf = (char *) malloc(strlen(req->path) + 16);
		jd_set_string(jd_get_ks(r, "path", 1), req->path);
		if(req->path[0] == '/' && req->path[1] == 0)
		{
			strcpy(pathbuf, "/index");
		}
		else
		{
			strcpy(pathbuf, req->path);
		}
		jd_set_string(jd_get_ks(r, "document", 1), pathbuf);
	}
	if(req->ext) jd_set_string(jd_get_ks(r, "ext", 1), req->ext);
	if(req->type) jd_set_string(jd_get_ks(r, "type", 1), req->type);
	if(req->host) jd_set_string(jd_get_ks(r, "host", 1), req->host);
	if(req->ident) jd_set_string(jd_get_ks(r, "ident", 1), req->ident);
	if(req->user) jd_set_string(jd_get_ks(r, "user", 1), req->user);
	if(req->method) jd_set_string(jd_get_ks(r, "method", 1), req->method);
	if(req->referer) jd_set_string(jd_get_ks(r, "referer", 1), req->referer);
	if(req->ua) jd_set_string(jd_get_ks(r, "ua", 1), req->ua);
	jd_assign(jd_get_ks(dict, "request", 1), r);
	jd_set_bool(jd_get_ks(dict, "home", 1), req->home);
	jd_set_bool(jd_get_ks(dict, "index", 1), req->index);
	if(req->indextitle)
	{
		jd_set_string(jd_get_ks(dict, "title", 1), req->indextitle);
	}
	if(pathbuf)
	{
		a = jd_nav(8);
		t = strchr(pathbuf, 0);
		*t = '.';
		t++;
/*		for(c = 0; quilt_typemap[c].ext; c++)
		{
			if(!quilt_typemap[c].visible)
			{
				continue;
			}
			if(req->type && !strcmp(req->type, quilt_typemap[c].type))
			{
				continue;
			}
			strcpy(t, quilt_typemap[c].ext);
			r = jd_nhv(3);
			jd_set_string(jd_get_ks(r, "type", 1), quilt_typemap[c].type);
			jd_set_string(jd_get_ks(r, "title", 1), quilt_typemap[c].name);
			jd_set_string(jd_get_ks(r, "uri", 1), pathbuf);
			jd_set_string(jd_get_ks(r, "ext", 1), quilt_typemap[c].ext);
			jd_assign(jd_push(a, 1), r);
			} */
		jd_assign(jd_get_ks(dict, "links", 1), a);
		free(pathbuf);
	}
	return 0;
}

/* Add the data contained in the request's librdf model to a 'data'
 * member of the dictionary.
 */
static int
add_data(jd_var *dict, QUILTREQ *req)
{
	jd_var *items, *item, *k, *props, *prop, *value;
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
	items = jd_nhv(25);
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
			item = jd_get_ks(items, uri, 0);			
			if(!item)
			{
				/* Populate a new item structure */
				item = jd_nhv(8);
				jd_set_bool(jd_get_ks(item, "me", 1), 0);
				add_subject(req, item, req->model, subj, uri);
				k = jd_get_ks(item, "props", 1);
				jd_assign(k, jd_nhv(8));
				k = jd_get_ks(items, uri, 1);
				jd_assign(k, item);
			}
			props = jd_get_ks(item, "props", 0);
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(pred));
			prop = jd_get_ks(props, uri, 0);
			if(!prop)
			{
				prop = jd_nav(4);
				jd_assign(jd_get_ks(props, uri, 1), prop);
			}
			value = jd_nhv(8);
			add_predicate(req, value, pred, uri);
			add_object(req, value, obj);
			k = jd_push(prop, 1);
			jd_assign(k, value);
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	librdf_free_statement(query);
	sbuf = (char *) calloc(1, strlen(req->subject) + 8);
	strcpy(sbuf, req->subject);
	strcat(sbuf, "#id");
	k = jd_get_ks(items, sbuf, 0);
	if(k)
	{
		jd_set_bool(jd_get_ks(k, "me", 1), 1);
		jd_assign(jd_get_ks(dict, "object", 1), k);
	}
	else
	{
		k = jd_get_ks(items, req->subject, 0);
		if(k)
		{
			jd_set_bool(jd_get_ks(k, "me", 1), 1);
			jd_assign(jd_get_ks(dict, "object", 1), k);
		}
		else
		{
			k = jd_get_ks(items, req->path, 0);
			if(k)
			{
				jd_set_bool(jd_get_ks(k, "me", 1), 1);
				jd_assign(jd_get_ks(dict, "object", 1), k);
			}
		}			
	}
	free(sbuf);
	jd_assign(jd_get_ks(dict, "data", 1), items);
	return 0;
}

/* Add the details of a specific subject to an 'item' structure which is
 * passed into the template.
 */
static int
add_subject(QUILTREQ *req, jd_var *item, librdf_model *model, librdf_node *subject, const char *uri)
{
	double lon, lat;
	jd_var *sub;
	struct class_struct *c;
	char *buf, *str;
	URI *uriobj;
	URI_INFO *info;

	uriobj = uri_create_str(uri, NULL);
	if(!uriobj)
	{
		quilt_logf(LOG_ERR, "failed to parse subject URI <%s>\n", uri);
		return -1;
	}
	info = uri_info(uriobj);

	jd_set_string(jd_get_ks(item, "subject", 1), uri);	
	if(!strncmp(uri, baseuri, baseurilen))
	{
		buf = (char *) malloc(strlen(uri) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uri[baseurilen]));
		jd_set_string(jd_get_ks(item, "link", 1), buf);
		jd_set_string(jd_get_ks(item, "uri", 1), buf);
	}
	else
	{
		buf = quilt_uri_contract(uri);
		jd_set_string(jd_get_ks(item, "link", 1), uri);
		jd_set_string(jd_get_ks(item, "uri", 1), buf);
	}	
	c = html_class_match(model, subject);
	str = get_title(req, model, subject);
	if(str)
	{
		jd_set_bool(jd_get_ks(item, "hasTitle", 1), 1);
		jd_set_string(jd_get_ks(item, "title", 1), str);
		free(str);
	}
	else
	{
		jd_set_bool(jd_get_ks(item, "hasTitle", 1), 0);
		jd_set_string(jd_get_ks(item, "title", 1), buf);
	}
	str = get_shortdesc(req, model, subject);
	if(str)
	{
		jd_set_string(jd_get_ks(item, "shortdesc", 1), str);
		free(str);
	}
	else
	{
		jd_set_string(jd_get_ks(item, "shortdesc", 1), "");
	}
	str = get_longdesc(req, model, subject);
	if(str)
	{
		jd_set_string(jd_get_ks(item, "description", 1), str);
		free(str);
	}
	else
	{
		jd_set_string(jd_get_ks(item, "description", 1), "");
	}
	if(buf[0] == '/' || !info->host)
	{
		jd_set_string(jd_get_ks(item, "from", 1), "");
	}
	else
	{
		strcpy(buf, "from ");
		strcpy(&(buf[5]), info->host);
			   jd_set_string(jd_get_ks(item, "from", 1), buf);
	}
		
	uri_info_destroy(info);
	free(buf);
	if(c)
	{
		jd_set_string(jd_get_ks(item, "class", 1), c->cssClass);
		jd_set_string(jd_get_ks(item, "classLabel", 1), c->label);
		jd_set_string(jd_get_ks(item, "classSuffix", 1), c->suffix);
		jd_set_string(jd_get_ks(item, "classDefinite", 1), c->definite);
	}
	else
	{
		jd_set_string(jd_get_ks(item, "class", 1), "");
		jd_set_string(jd_get_ks(item, "classSuffix", 1), "");
	}
	
	if(quilt_model_find_double(model, uri, "http://www.w3.org/2003/01/geo/wgs84_pos#long", &lon) == 1 &&
	   quilt_model_find_double(model, uri, "http://www.w3.org/2003/01/geo/wgs84_pos#lat", &lat) == 1)
	{
		sub = jd_nhv(2);
		jd_set_real(jd_get_ks(sub, "long", 1), lon);
		jd_set_real(jd_get_ks(sub, "lat", 1), lat);
		jd_assign(jd_get_ks(item, "geo", 1), sub);
	}
	return 0;
}

/* Add the details of a specific predicate to a 'value' structure which is
 * passed into the template.
 */
static int
add_predicate(QUILTREQ *req, jd_var *value, librdf_node *predicate, const char *uri)
{
	char *contracted;

	(void) req;
	(void) predicate;

	jd_set_string(jd_get_ks(value, "predicateUri", 1), uri);
	contracted = quilt_uri_contract(uri);
	jd_set_string(jd_get_ks(value, "predicateUriLabel", 1), contracted);
	free(contracted);
	return 0;
}

/* Add the details of a triple's object to a 'value' structure which is
 * passed into the template
 */
static int
add_object(QUILTREQ *req, jd_var *value, librdf_node *object)
{
	const char *str, *dtstr, *lang;
	char *buf;
	librdf_uri *dt;

	(void) req;

	if(librdf_node_is_resource(object))
	{
		str = (const char *) librdf_uri_as_string(librdf_node_get_uri(object));
		jd_set_string(jd_get_ks(value, "type", 1), "uri");
		jd_set_bool(jd_get_ks(value, "isUri", 1), 1);
		jd_set_string(jd_get_ks(value, "value", 1), str);
		if(!strncmp(str, baseuri, baseurilen))
		{
			buf = (char *) malloc(strlen(str) + 32);
			buf[0] = '/';
			strcpy(&(buf[1]), &(str[baseurilen]));
			jd_set_string(jd_get_ks(value, "link", 1), buf);
			jd_set_string(jd_get_ks(value, "uri", 1), buf);
		}
		else
		{
			buf = quilt_uri_contract(str);
			jd_set_string(jd_get_ks(value, "uri", 1), buf);
			jd_set_string(jd_get_ks(value, "link", 1), str);
		}
		free(buf);
	}
	else if(librdf_node_is_literal(object))
	{
		str = (const char *) librdf_node_get_literal_value(object);
		
		jd_set_string(jd_get_ks(value, "type", 1), "literal");
		jd_set_bool(jd_get_ks(value, "isLiteral", 1), 1);
		jd_set_string(jd_get_ks(value, "value", 1), str);

		lang = librdf_node_get_literal_value_language(object);
		if(lang)
		{
			jd_set_string(jd_get_ks(value, "lang", 1), lang);
		}
		dt = librdf_node_get_literal_value_datatype_uri(object);
		if(dt)
		{
			dtstr = (const char *) librdf_uri_as_string(dt);
			if(dtstr)
			{
				jd_set_string(jd_get_ks(value, "datatype", 1), dtstr);
				buf = quilt_uri_contract(dtstr);
				jd_set_string(jd_get_ks(value, "datatypeUri", 1), buf);
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
		quilt_logf(LOG_ERR, "failed to create new URI for <%s>\n", predicate);
		return NULL;
	}
	query = librdf_new_statement(world);
	if(!query)
	{
		quilt_logf(LOG_ERR, "failed to create new statement for query\n");
		librdf_free_node(obj);
		return NULL;
	}
	subject = librdf_new_node_from_node(subject);
	if(!subject)
	{
		quilt_logf(LOG_ERR, "failed to duplicate subject node\n");
		librdf_free_node(obj);
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, subject);
	librdf_statement_set_predicate(query, obj);
	stream = librdf_model_find_statements(model, query);
	if(!stream)
	{
		quilt_logf(LOG_ERR, "failed to create stream for model query\n");
		librdf_free_statement(query);
		return NULL;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		obj = librdf_statement_get_object(st);
		if(!obj)
		{
			quilt_logf(LOG_ERR, "failed to obtain object of statement\n");
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
	quilt_logf(LOG_DEBUG, "loading template: '%s'\n", path);
	f = fopen(path, "rb");
	if(!f)
	{
		quilt_logf(LOG_ERR, "%s: (failed to open) %s\n", path, strerror(errno));
		return NULL;
	}
	while(1)
	{
		p = (char *) realloc(buf, alloc + 512);
		if(!p)
		{
			quilt_logf(LOG_ERR, "%s: %s\n", path, strerror(errno));
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
		quilt_logf(LOG_CRIT, "failed to allocate pathname buffer\n");
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
