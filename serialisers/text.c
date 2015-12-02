/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

#include "p_text.h"

static int text_serialize(QUILTREQ *req);
static int text_serialize_stream(QUILTREQ *req, librdf_node *context, librdf_stream *stream);
static int text_serialize_subject(QUILTREQ *req, librdf_node *context, librdf_node *subject);
static int text_serialize_uri(QUILTREQ *req, librdf_uri *uri);
static int text_serialize_node(QUILTREQ *req, librdf_node *node);
static char *text_node_string(QUILTREQ *req, librdf_node *node);

static QUILTTYPE text_types[] = {
	{ "text/plain", "text txt", "Plain text", 0.95f, 1, NULL },
	{ NULL, NULL, NULL, 0, 0, NULL },
};

int
quilt_plugin_init(void)
{
	size_t c;

	for(c = 0; text_types[c].mimetype; c++)
	{
		quilt_plugin_register_serializer(&(text_types[c]), text_serialize);
	}
	return 0;
}

static int
text_serialize(QUILTREQ *req)
{
	char *loc;
	librdf_model *model;
	librdf_iterator *iter;
	librdf_node *context;
	librdf_stream *stream;
	
	loc = quilt_canon_str(quilt_request_canonical(req), QCO_CONCRETE|QCO_NOABSOLUTE);
	quilt_request_headerf(req, "Status: %d %s\n", quilt_request_status(req), quilt_request_statustitle(req));
	quilt_request_headers(req, "Content-Type: text/plain; charset=utf-8\n");
	quilt_request_headerf(req, "Content-Location: %s\n", loc);
	quilt_request_headers(req, "Vary: Accept\n");
	quilt_request_headers(req, "Server: " PACKAGE_SIGNATURE "\n");
	free(loc);

	model = quilt_request_model(req);
	iter = librdf_model_get_contexts(model);
	if(librdf_iterator_end(iter))
	{
		librdf_free_iterator(iter);
		stream = librdf_model_as_stream(model);
		text_serialize_stream(req, NULL, stream);
	}
	else
	{
		while(!librdf_iterator_end(iter))
		{
			context = librdf_iterator_get_object(iter);
			quilt_request_puts(req, "According to ");
			text_serialize_node(req, context);
			quilt_request_puts(req, ":\n\n");
			stream = librdf_model_context_as_stream(model, context);
			text_serialize_stream(req, context, stream);
			librdf_free_stream(stream);
			librdf_iterator_next(iter);
		}
		librdf_free_iterator(iter);
	}
	return 0;
}

static int
text_serialize_node(QUILTREQ *req, librdf_node *node)
{
	char *p;
	
	if(librdf_node_is_resource(node))
	{
		return text_serialize_uri(req, librdf_node_get_uri(node));
	}
	p = text_node_string(req, node);
	quilt_request_puts(req, p);
	free(p);
	return 0;
}

static int
text_serialize_uri(QUILTREQ *req, librdf_uri *uri)
{
	const char *str;
	char *contracted;

	str = (const char *) librdf_uri_as_string(uri);
	contracted = quilt_uri_contract(str);
	if(strcmp(contracted, str))
	{		
		quilt_request_puts(req, contracted);
	}
	else
	{
		quilt_request_printf(req, "<%s>", str);
	}
	free(contracted);
	return 0;
}



static int
text_serialize_stream(QUILTREQ *req, librdf_node *context, librdf_stream *stream)
{
	char *s;
	librdf_world *world;
	librdf_node *subject;
	librdf_statement *statement;
	librdf_hash *hash;

	world = quilt_librdf_world();
	hash = librdf_new_hash(world, NULL);
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		subject = librdf_statement_get_subject(statement);
		s = text_node_string(req, subject);
		if(librdf_hash_get_as_boolean(hash, s) < 1)
		{
			librdf_hash_put_strings(hash, s, "yes");
			quilt_request_puts(req, "  ");
			text_serialize_node(req, subject);
			text_serialize_subject(req, context, subject);
		}
		free(s);
		librdf_stream_next(stream);
	}
	librdf_free_hash(hash);
	quilt_request_puts(req, "\n");
	return 0;
}

static int
text_serialize_subject(QUILTREQ *req, librdf_node *context, librdf_node *subject)
{
	librdf_world *world;
	librdf_model *model;
	librdf_hash *hash;
	librdf_statement *query, *statement;
	librdf_node *object;
	librdf_stream *stream;
	size_t c;

	world = quilt_librdf_world();
	model = quilt_request_model(req);
	hash = librdf_new_hash(world, NULL);
	librdf_hash_put_strings(hash, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "yes");
	/* Always emit rdf:type first */
	query = librdf_new_statement(world);
	librdf_statement_set_subject(query, librdf_new_node_from_node(subject));
	librdf_statement_set_predicate(query, quilt_node_create_uri("http://www.w3.org/1999/02/22-rdf-syntax-ns#type"));
	if(context)
	{
		stream = librdf_model_find_statements_with_options(model, query, context, NULL);
	}
	else
	{
		stream = librdf_model_find_statements(model, query);
	}
	c = 0;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		object = librdf_statement_get_object(statement);
		if(c)
		{
			quilt_request_puts(req, ", ");
		}
		else
		{
			quilt_request_puts(req, " is a ");
		}
		c++;
		text_serialize_node(req, object);
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);   
	if(c)
	{
		quilt_request_puts(req, ".\n");
	}
	else
	{
		quilt_request_puts(req, ":\n");
	}
	librdf_free_hash(hash);
	quilt_request_puts(req, "\n");
	return 0;
}

static char *
text_node_string(QUILTREQ *req, librdf_node *node)
{
	raptor_world *world;
	raptor_iostream *stream;
	char *buf;
	int r;

	world = librdf_world_get_raptor(librdf_storage_get_world(quilt_request_storage(req)));
	buf = NULL;
	stream = raptor_new_iostream_to_string(world, (void **) &buf, NULL, malloc);
	if(!stream)
	{
		return NULL;
	}
	r = librdf_node_write(node, stream);
	raptor_free_iostream(stream);
	if(r)
	{
		free(buf);
		return NULL;
	}
	return buf;
}
