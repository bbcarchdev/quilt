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

#include "p_quilt.h"

#ifndef LIBRDF_MODEL_FEATURE_CONTEXTS
# error librdf library version is too old; please upgrade to a version which supports contexts
#endif

static librdf_world *quilt_world = NULL;

static int quilt_librdf_logger_(void *data, librdf_log_message *message);
static int quilt_ns_cb_(const char *key, const char *value, void *data);

/* Initialise the librdf execution context, quilt_world */
int
quilt_librdf_init(void)
{
	const raptor_syntax_description *desc;
	unsigned int c, i;
	float q;

	if(!quilt_world)
	{
		quilt_world = librdf_new_world();
		if(!quilt_world)
		{
			log_printf(LOG_CRIT, "failed to create new RDF world\n");
			return -1;
		}
		librdf_world_open(quilt_world);
		librdf_world_set_logger(quilt_world, NULL, quilt_librdf_logger_);
		for(c = 0; (desc = librdf_serializer_get_description(quilt_world, c)); c++)
		{

			for(i = 0; i < desc->mime_types_count; i++)
			{
				q = (float) desc->mime_types[i].q / 10.0f;
				/* Cap certain specific types */
				if(!strcmp(desc->mime_types[i].mime_type, "application/rdf+xml") && q > 0.75)
				{
					q = 0.75;
				}
				else if(!strcmp(desc->mime_types[i].mime_type, "application/n-triples") && q > 0.75)
				{
					q = 0.75;
				}
				else if(q > 0.85)
				{
					q = 0.85;
				}
				neg_add(quilt_types, desc->mime_types[i].mime_type, q);
				log_printf(LOG_DEBUG, "adding '%s' with q=%f\n", desc->mime_types[i].mime_type, q);
			}
		}		
		/* Artifically inflate the q-values of some types */
		neg_add(quilt_types, "text/turtle", 0.9f);
		neg_add(quilt_types, "text/html", 0.95f);
		neg_add(quilt_charsets, "utf-8", 1);
	}
	return 0;
}

/* Obtain the librdf world */
librdf_world *
quilt_librdf_world(void)
{
	if(quilt_librdf_init())
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

	if(quilt_librdf_init())
	{
		return -1;
	}
	if(!base)
	{
		base = librdf_new_uri(quilt_world, (const unsigned char *) "/");
		if(!base)
		{
			log_printf(LOG_CRIT, "failed to parse URI </>\n");
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
		log_printf(LOG_ERR, "failed to create a new parser for %s (%s)\n", mime, name);
		return -1;
	}
	r = librdf_parser_parse_counted_string_into_model(parser, (const unsigned char *) buf, buflen, base, model);
	librdf_free_parser(parser);
	return r;
}

char *
quilt_model_serialize(librdf_model *model, const char *mime)
{
	char *buf;
	librdf_serializer *serializer;
	const char *name;

	if(quilt_librdf_init())
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
		log_printf(LOG_ERR, "failed to create a new serializer for %s (%s)\n", mime, name);
		return NULL;
	}

	config_get_all("namespaces", NULL, quilt_ns_cb_, (void *) serializer);

	buf = (char *) librdf_serializer_serialize_model_to_string(serializer, NULL, model);
	librdf_free_serializer(serializer);
	return buf;
}

static int
quilt_ns_cb_(const char *key, const char *value, void *data)
{
	librdf_serializer *serializer;
	const char *prefix = "namespaces:";
	size_t l;
	librdf_uri *uri;

	serializer = (librdf_serializer *) data;
	l = strlen(prefix);
	if(strncmp(key, prefix, l))
	{
		return 0;
	}
	key += l;
	uri = librdf_new_uri(quilt_world, (const unsigned char *) value);
	if(!uri)
	{
		log_printf(LOG_ERR, "failed to create new URI from <%s>\n", value);
		return 0;
	}
	librdf_serializer_set_namespace(serializer, uri, key);
	librdf_free_uri(uri);
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
	log_printf(level, "%s\n", librdf_log_message_message(message));
	return 0;
}
