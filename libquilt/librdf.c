/* Quilt: librdf wrappers
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

#include "p_libquilt.h"

#ifndef LIBRDF_MODEL_FEATURE_CONTEXTS
# error librdf library version is too old; please upgrade to a version which supports contexts
#endif

struct namespace_struct
{
	char *prefix;
	char *uri;
	size_t len;
};

static librdf_world *quilt_world;
static struct namespace_struct *namespaces;
static size_t nscount;

static int quilt_librdf_serialize_(QUILTREQ *request);
static int quilt_librdf_logger_(void *data, librdf_log_message *message);
static int quilt_ns_cb_(const char *key, const char *value, void *data);

/* Initialise the librdf execution context, quilt_world */
int
quilt_librdf_init_(void)
{
	QUILTTYPE type;
	const raptor_syntax_description *desc;
	unsigned int c, i;

	if(!quilt_world)
	{
		quilt_logf(LOG_DEBUG, "initialising librdf wrapper\n");
		quilt_world = librdf_new_world();
		if(!quilt_world)
		{
			quilt_logf(LOG_CRIT, "failed to create new RDF world\n");
			return -1;
		}
		librdf_world_open(quilt_world);
		librdf_world_set_logger(quilt_world, NULL, quilt_librdf_logger_);
		/* Obtain all of our namespaces from the configuration */
		config_get_all("namespaces", NULL, quilt_ns_cb_, NULL);
		/* Register our MIME types for the built-in serializer */
		for(c = 0; (desc = librdf_serializer_get_description(quilt_world, c)); c++)
		{

			for(i = 0; i < desc->mime_types_count; i++)
			{
				memset(&type, 0, sizeof(type));
				type.mimetype = desc->mime_types[i].mime_type;
				type.qs = (float) desc->mime_types[i].q / 10.0f;
				/* Cap certain specific types */
				if(!strcmp(desc->mime_types[i].mime_type, "application/rdf+xml") && type.qs > 0.75)
				{
					type.qs = 0.75;
				}
				else if(!strcmp(desc->mime_types[i].mime_type, "application/n-triples") && type.qs > 0.75)
				{
					type.qs = 0.75;
				}
				else if(type.qs > 0.85)
				{
					type.qs = 0.85;
				}
				/* Force the qs-value of text/turtle to be high */
				if(!strcmp(type.mimetype, "text/turtle"))
				{
					type.qs = 0.9f;
				}
				if(quilt_plugin_register_serializer(&type, quilt_librdf_serialize_))
				{
					quilt_logf(LOG_ERR, "failed to register MIME type '%s'\n", type.mimetype);
				}
			}
		}
		quilt_logf(LOG_DEBUG, "librdf wrapper initialised\n");
		/* Artifically inflate the q-values of some types */
/*		neg_add(quilt_types, "text/turtle", 0.9f);
  neg_add(quilt_types, "text/html", 0.95f); */
	}
	return 0;
}

/* The built-in RDF serializer */
static
int quilt_librdf_serialize_(QUILTREQ *request)
{
	const char *tsuffix;
	char *buf;
	
	buf = quilt_model_serialize(request->model, request->type);
	if(!buf)
	{
		quilt_logf(LOG_ERR, "failed to serialise model as %s\n", request->type);
		return 406;
	}	
	if(!strncmp(request->type, "text/", 5))
	{
		tsuffix = "; charset=utf-8";
	}
	else
	{
		tsuffix = "";
	}
	FCGX_FPrintF(request->fcgi->out, "Status: 200 OK\n"
				 "Content-type: %s%s\n"
				 "Vary: Accept\n"
				 "Server: Quilt\n"
				 "\n", request->type, tsuffix);
	FCGX_PutStr(buf, strlen(buf), request->fcgi->out);
	free(buf);
	return 0;	
}

/* Obtain the librdf world */
librdf_world *
quilt_librdf_world(void)
{
	if(quilt_librdf_init_())
	{
		return NULL;
	}
	return quilt_world;
}

/* Parse a buffer of a particular MIME type into a model */
int
quilt_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen)
{	
	static librdf_uri *base;
	const char *name;
	librdf_parser *parser;
	int r;

	if(quilt_librdf_init_())
	{
		return -1;
	}
	if(!base)
	{
		base = librdf_new_uri(quilt_world, (const unsigned char *) "/");
		if(!base)
		{
			quilt_logf(LOG_CRIT, "failed to parse URI </>\n");
			return -1;
		}
	}
	name = NULL;
	/* Handle specific MIME types whether or not librdf already knows
	 * about them
	 */
	if(!strcmp(mime, "application/trig"))
	{
		name = "trig";
	}
	else if(!strcmp(mime, "application/nquads"))
	{
		name = "nquads";
	}
	/* If we have a specific parser name, don't use the MIME type */
	if(name)
	{
		mime = NULL;
	}
	parser = librdf_new_parser(quilt_world, name, mime, NULL);
	if(!parser)
	{
		if(!name)
		{
			name = "auto";
		}
		quilt_logf(LOG_ERR, "failed to create a new parser for %s (%s)\n", mime, name);
		return -1;
	}
	r = librdf_parser_parse_counted_string_into_model(parser, (const unsigned char *) buf, buflen, base, model);
	librdf_free_parser(parser);
	return r;
}

char *
quilt_model_serialize(librdf_model *model, const char *mime)
{
	size_t c;
	char *buf;
	librdf_serializer *serializer;
	librdf_uri *uri;
	const char *name;

	if(quilt_librdf_init_())
	{
		return NULL;
	}
	name = NULL;
	/* Handle specific MIME types whether or not librdf already knows
	 * about them
	 */
	if(!strcmp(mime, "application/trig"))
	{
		name = "trig";
	}
	else if(!strcmp(mime, "application/nquads"))
	{
		name = "nquads";
	}
	else if(!strcmp(mime, "application/rdf+xml"))
	{
		name = "rdfxml-abbrev";
	}
	else if(!strcmp(mime, "text/html"))
	{
		name = "html";
	}
	/* If we have a specific serializer name, don't use the MIME type */
	if(name)
	{
		mime = NULL;
	}
	serializer = librdf_new_serializer(quilt_world, name, mime, NULL);
	if(!serializer)
	{
		if(!name)
		{
			name = "auto";
		}
		quilt_logf(LOG_ERR, "failed to create a new serializer for %s (%s)\n", mime, name);
		return NULL;
	}
	for(c = 0; namespaces[c].prefix; c++)
	{
		uri = librdf_new_uri(quilt_world, (const unsigned char *) namespaces[c].uri);
		if(!uri)
		{
			quilt_logf(LOG_ERR, "failed to create new URI from <%s>\n", namespaces[c].uri);
			return 0;
		}
		librdf_serializer_set_namespace(serializer, uri, namespaces[c].prefix);
		librdf_free_uri(uri);
	}
	buf = (char *) librdf_serializer_serialize_model_to_string(serializer, NULL, model);
	librdf_free_serializer(serializer);
	return buf;
}

static int
quilt_ns_cb_(const char *key, const char *value, void *data)
{
	const char *prefix = "namespaces:";
	size_t l;
	struct namespace_struct *p;

	(void) data;

	l = strlen(prefix);
	if(strncmp(key, prefix, l))
	{
		return 0;
	}
	key += l;
	p = (struct namespace_struct *) realloc(namespaces, sizeof(struct namespace_struct) * (nscount + 2));
	if(!p)
	{
		return -1;
	}
	namespaces = p;
	namespaces[nscount].prefix = strdup(key);
	namespaces[nscount].uri = strdup(value);
	namespaces[nscount].len = strlen(value);
	nscount++;
	namespaces[nscount].prefix = NULL;
	namespaces[nscount].uri = NULL;
	namespaces[nscount].len = 0;
	return 0;
}

int
quilt_model_isempty(librdf_model *model)
{
	librdf_stream *stream;
	int r;

	stream = librdf_model_as_stream(model);
	if(!stream)
	{
		return -1;
	}
	if(librdf_stream_end(stream))
	{
		r = 1;
	}
	else
	{
		r = 0;
	}
	librdf_free_stream(stream);
	return r;
}

/* Attempt to contract a URI to prefix:suffix form */
char *
quilt_uri_contract(const char *uri)
{
	char *p;
	const char *prefix;
	size_t len, c;

	len = 0;
	prefix = NULL;
	
	for(c = 0; namespaces[c].prefix; c++)
	{
		if(namespaces[c].len > len &&
		   !strncmp(uri, namespaces[c].uri, namespaces[c].len))
		{
			prefix = namespaces[c].prefix;
			len = namespaces[c].len;
		}
	}
	if(prefix)
	{
		p = (char *) malloc(strlen(uri) + strlen(prefix) + 2);
		if(!p)
		{
			return NULL;
		}
		strcpy(p, prefix);
		strcat(p, ":");
		strcat(p, &(uri[len]));
		return p;
	}		
	return strdup(uri);
}

/* Log events from librdf */
static int
quilt_librdf_logger_(void *data, librdf_log_message *message)
{
	int level;

	(void) data;
	
	switch(librdf_log_message_level(message))
	{
	case LIBRDF_LOG_DEBUG:
		level = LOG_DEBUG;
		break;
	case LIBRDF_LOG_INFO:
		level = LOG_INFO;
		break;
	case LIBRDF_LOG_WARN:
		level = LOG_WARNING;
		break;
	case LIBRDF_LOG_ERROR:
		level = LOG_ERR;
		break;
	case LIBRDF_LOG_FATAL:
		level = LOG_CRIT;
		break;
	default:
		level = LOG_NOTICE;
		break;
	}
	quilt_logf(level, "%s\n", librdf_log_message_message(message));
	return 0;
}

librdf_node *
quilt_node_create_uri(const char *uri)
{
	librdf_node *node;

	if(quilt_librdf_init_())
	{
		return NULL;
	}
	/* TODO: expand URIs using known namespaces if needed */
	node = librdf_new_node_from_uri_string(quilt_world, (const unsigned char *)uri);
	if(!node)
	{
		quilt_logf(LOG_ERR, "failed to create node for <%s>\n", uri);
		return NULL;
	}
	return node;
}

librdf_node *
quilt_node_create_literal(const char *value, const char *lang)
{
	librdf_node *node;

	if(quilt_librdf_init_())
	{
		return NULL;
	}
	node = librdf_new_node_from_literal(quilt_world, (const unsigned char *) value, lang, 0);
	if(!node)
	{
		quilt_logf(LOG_ERR, "failed to create node for literal value\n");
		return NULL;
	}
	return node;
}

librdf_statement *
quilt_st_create(const char *subject, const char *predicate)
{
	librdf_statement *st;
	librdf_node *node;

	if(quilt_librdf_init_())
	{
		return NULL;
	}	
	st = librdf_new_statement(quilt_world);
	if(!st)
	{
		quilt_logf(LOG_ERR, "failed to create a new RDF statement\n");
		return NULL;
	}	
	node = quilt_node_create_uri(subject);
	if(!node)
	{
		librdf_free_statement(st);
		return NULL;
	}
	librdf_statement_set_subject(st, node);
	
	node = quilt_node_create_uri(predicate);
	if(!node)
	{
		librdf_free_statement(st);
		return NULL;
	}	
	librdf_statement_set_predicate(st, node);
	return st;
}

librdf_statement *
quilt_st_create_literal(const char *subject, const char *predicate, const char *value, const char *lang)
{
	librdf_statement *st;
	librdf_node *node;

	st = quilt_st_create(subject, predicate);
	if(!st)
	{
		return NULL;
	}
	node = quilt_node_create_literal(value, lang);
	if(!node)
	{
		librdf_free_statement(st);
		return NULL;
	}
	librdf_statement_set_object(st, node);
	return st;
}

librdf_statement *
quilt_st_create_uri(const char *subject, const char *predicate, const char *value)
{
	librdf_statement *st;
	librdf_node *node;
	
	st = quilt_st_create(subject, predicate);
	if(!st)
	{
		return NULL;
	}
	node = quilt_node_create_uri(value);
	if(!node)
	{
		librdf_free_statement(st);
		return NULL;
	}
	librdf_statement_set_object(st, node);
	return st;
}

int
quilt_model_find_double(librdf_model *model, const char *subject, const char *predicate, double *result)
{
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *obj;
	librdf_uri *dturi;
	char *endp;
	const char *value, *dtstr;
	int found;

	found = 0;
	*result = 0;
	query = quilt_st_create(subject, predicate);
	if(!query)
	{
		return -1;
	}
	stream = librdf_model_find_statements(model, query);
	if(!stream)
	{
		quilt_logf(LOG_ERR, "failed to create RDF stream for query\n");
		librdf_free_statement(query);
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		obj = librdf_statement_get_object(st);
		if(librdf_node_is_literal(obj) &&
		   (value = (const char *) librdf_node_get_literal_value(obj)) &&
		   (dturi = librdf_node_get_literal_value_datatype_uri(obj)) &&
		   (dtstr = (const char *) librdf_uri_as_string(dturi)))
		{
			if(!strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#decimal"))
			{
				endp = NULL;
				*result = strtod(value, &endp);
				if(!endp || !endp[0])
				{
					found = 1;
					break;
				}
			}
				
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return found;
}

