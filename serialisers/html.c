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

static char *baseuri;
static size_t baseurilen;

static int html_serialize(QUILTREQ *req);
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
	if(html_template_init())
	{
		free(baseuri);
		baseuri = NULL;
		return -1;
	}
	for(c = 0; html_types[c].mimetype; c++)
	{
		quilt_plugin_register_serializer(&(html_types[c]), html_serialize);
	}
	return 0;
}

static int
html_serialize(QUILTREQ *req)
{
	QUILTCANON *canon;
	LIQUIFYTPL *tpl;
	json_t *dict;
	char *buf, *loc;
	int status;

	status = 500;
	canon = quilt_request_canonical(req);
	dict = json_object();
	html_add_common(dict, req);
	add_req(dict, req);
	add_data(dict, req);
	/*	json_dumpf(dict, stderr, JSON_INDENT(4)); 
		exit(0); */
	tpl = html_template(req);
	if(tpl)
	{
		/* Set status to zero to suppress output */
		status = 0;
		buf = liquify_apply(tpl, dict);
		loc = quilt_canon_str(canon, QCO_CONCRETE|QCO_NOABSOLUTE);
		quilt_request_headerf(req, "Status: %d %s\n", quilt_request_status(req), quilt_request_statustitle(req));
		quilt_request_headerf(req, "Content-Type: %s; charset=utf-8\n", quilt_request_type(req));
		quilt_request_headerf(req, "Content-Location: %s\n", loc);
		quilt_request_headers(req, "Vary: Accept\n");
		quilt_request_headers(req, "Server: " PACKAGE_SIGNATURE "\n");
		free(loc);
		quilt_request_puts(req, buf);
		free(buf);
	}
	json_decref(dict);
	return status;
}

/* Add the details of req to a 'request' member of the dictionary */
static int
add_req(json_t *dict, QUILTREQ *req)
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
	librdf_model *model;
	const char *uri;
	QUILTCANON *reqcanon;
	char *canon;
	int n;

	model = quilt_request_model(req);
	world = quilt_librdf_world();
	reqcanon = quilt_request_canonical(req);
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
	st = librdf_model_find_statements(model, query);
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
				add_subject(req, item, model, subj, uri);
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
	n = 0;
	canon = quilt_canon_str(reqcanon, QCO_NOEXT|QCO_FRAGMENT);
	if(strchr(canon, '#'))
	{
		item = json_object_get(items, canon);
		if(item)
		{
			json_object_set_new(item, "me", json_true());
			json_object_set(dict, "object", item);
			if((k = json_object_get(item, "title")))
			{
				json_object_set(dict, "title", k);
			}
			n = 1;
		}
	}
	free(canon);
	if(!n)
	{
		canon = quilt_canon_str(reqcanon, (quilt_request_ext(req) ? QCO_ABSTRACT : QCO_REQUEST));
		item = json_object_get(items, canon);
		if(item)
		{
			json_object_set_new(item, "me", json_true());
			json_object_set(dict, "object", item);
			if((k = json_object_get(item, "title")))
			{
				json_object_set(dict, "title", k);
			}
			n = 1;
		}
		free(canon);
	}
	if(!n)
	{
		canon = quilt_canon_str(reqcanon, QCO_CONCRETE);
		item = json_object_get(items, quilt_request_path(req));
		if(item)
		{
			json_object_set_new(item, "me", json_true());
			json_object_set(dict, "object", item);
			if((k = json_object_get(item, "title")))
			{
				json_object_set(dict, "title", k);
			}
		}
		free(canon);
	}
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
