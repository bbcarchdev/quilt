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

#include "p_quilt.h"

struct index_struct
{
	const char *uri;
	const char *title;
	const char *qclass;
};

/* XXX replace with config */
static struct index_struct indices[] = {
	{ "/everything", "Everything", NULL },
	{ "/people", "People", "http://xmlns.com/foaf/0.1/Person" },
	{ "/groups", "Groups", "http://xmlns.com/foaf/0.1/Group" },
	{ "/agents", "Agents", "http://xmlns.com/foaf/0.1/Agent" },
	{ "/places", "Places", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ "/events", "Events", "http://purl.org/NET/c4dm/event.owl#Event" },
	{ "/things", "Physical things", "http://www.cidoc-crm.org/cidoc-crm/E18_Physical_Thing" },
	{ "/collections", "Collections", "http://purl.org/dc/dcmitype/Collection" },
	{ "/works", "Creative works", "http://purl.org/vocab/frbr/core#Work" },
	{ "/assets", "Digital assets", "http://xmlns.com/foaf/0.1/Document" },
	{ "/concepts", "Concepts", "http://www.w3.org/2004/02/skos/core#Concept" },
	{ NULL, NULL, NULL }
};

static int coref_index(QUILTREQ *req, const char *qclass);
static int coref_home(QUILTREQ *req);
static int coref_item(QUILTREQ *req);
static int coref_lookup(QUILTREQ *req, const char *uri);
static int coref_index_metadata_sparqlres(QUILTREQ *request, SPARQLRES *res);
static int coref_index_metadata_stream(QUILTREQ *request, librdf_stream *stream, int subjobj);

/* This engine retrieves coreference information as stored by Spindle */
int
quilt_engine_coref_process(QUILTREQ *request)
{
	char *qclass;
	const char *t;
	size_t c;
	int r;
		
	qclass = NULL;
	t = FCGX_GetParam("class", request->query);
	if(t)
	{
		qclass = (char *) calloc(1, 32 + strlen(t));
		sprintf(qclass, "FILTER ( ?class = <%s> )", t);
		request->indextitle = t;
		request->index = 1;
		request->home = 0;
	}
	else
	{
		for(c = 0; indices[c].uri; c++)
		{
			if(!strcmp(request->path, indices[c].uri))
			{
				if(indices[c].qclass)
				{
					qclass = (char *) calloc(1, 32 + strlen(indices[c].qclass));
					sprintf(qclass, "FILTER ( ?class = <%s> )", indices[c].qclass);
				}
				request->indextitle = indices[c].title;
				request->index = 1;
			}
		}
	}
	if(request->home)
	{
		r = coref_home(request);
	}
	else if(request->index)
	{
		r = coref_index(request, qclass);
	}   
	else
	{
		r = coref_item(request);
	}
	free(qclass);
	return r;
}

static int
coref_index(QUILTREQ *request, const char *qclass)
{
	SPARQL *sparql;
	SPARQLRES *res;
	char limofs[128];
	librdf_statement *st;
	int r;

	if(request->offset)
	{
		snprintf(limofs, sizeof(limofs) - 1, "OFFSET %d LIMIT %d", request->offset, request->limit);
	}
	else
	{
		snprintf(limofs, sizeof(limofs) - 1, "LIMIT %d", request->limit);
	}	
	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}
	res = sparql_queryf(sparql, "SELECT DISTINCT ?s\n"
						"WHERE {\n"
						" GRAPH <%s> {\n"
						"  ?s <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> ?class .\n"
						"  %s"
						"}\n"
						" GRAPH ?g {\n"
						"  ?s <http://purl.org/dc/terms/modified> ?modified\n"
						" }\n"
						"}\n"
						"ORDER BY DESC(?modified)\n"
						"%s",
						request->base, ( qclass ? qclass : "" ), limofs);
	if(!res)
	{
		log_printf(LOG_ERR, "SPARQL query for index subjects failed\n");
		return 500;
	}
	if((r = coref_index_metadata_sparqlres(request, res)))
	{
		sparqlres_destroy(res);
		return r;
	}
	sparqlres_destroy(res);	
	st = quilt_st_create_literal(request->path, "http://www.w3.org/2000/01/rdf-schema#label", request->indextitle, "en");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	/* Return 0, rather than 200, to auto-serialise the model */
	return 0;
}

/* For all of the things matching a particular query, add the metadata from
 * the related graphs to the model.
 */
static int
coref_index_metadata_sparqlres(QUILTREQ *request, SPARQLRES *res)
{
	char *query, *p;
	size_t buflen;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	int subj;
	const char *uristr;
	librdf_statement *st;

	buflen = 128 + strlen(request->base);
	query = (char *) calloc(1, buflen);
	/*
	  PREFIX ...
	  SELECT ?s ?p ?o ?g WHERE {
	  GRAPH ?g {
	  ?s ?p ?o .
	  FILTER( ...subjects... )
	  FILTER( ...predicates... )
	  }
	  }
	*/
	sprintf(query, "SELECT ?s ?p ?o ?g WHERE { GRAPH ?g { ?s ?p ?o . FILTER(?g != <%s>) FILTER(", request->base);
	subj = 0;
	while((row = sparqlres_next(res)))
	{
		node = sparqlrow_binding(row, 0);
		if(!librdf_node_is_resource(node))
		{
			continue;
		}
		if((uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", uristr);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st);
			buflen += 8 + (strlen(uristr) * 3);
			p = (char *) realloc(query, buflen);
			if(!p)
			{
				log_printf(LOG_CRIT, "failed to reallocate buffer to %lu bytes\n", (unsigned long) buflen);
				return 500;
			}
			query = p;
			p = strchr(query, 0);
			strcpy(p, "?s = <");
			for(p = strchr(query, 0); *uristr; uristr++)
			{
				if(*uristr == '>')
				{
					*p = '%';
					p++;
					*p = '3';
					p++;
					*p = 'e';
				}
				else
				{
					*p = *uristr;
				}
				p++;
			}
			*p = 0;
			strcpy(p, "> ||");
			subj = 1;
		}
	}
	if(subj)
	{
		strcpy(p, "> ) } }");
		if(quilt_sparql_query_rdf(query, request->model))
		{
			log_printf(LOG_ERR, "failed to create model from query\n");
			free(query);
			return 500;
		}
	}
	free(query);
	return 0;
}

static int
coref_index_metadata_stream(QUILTREQ *request, librdf_stream *stream, int subjobj)
{
	char *query, *p;
	size_t buflen;
	librdf_node *node;
	librdf_uri *uri;
	int subj;
	const char *uristr;
	librdf_statement *st;

	buflen = 256 + strlen(request->base);
	query = (char *) calloc(1, buflen);
	/*
	  PREFIX ...
	  SELECT ?s ?p ?o ?g WHERE {
	  GRAPH ?g {
	  ?s ?p ?o .
	  FILTER( ...subjects... )
	  FILTER( ...predicates... )
	  }
	  }
	*/
	sprintf(query, "SELECT ?s ?p ?o ?g WHERE { GRAPH ?g { ?s ?p ?o . FILTER(?g != <%s> && ?g != <%s>) FILTER(", request->subject, request->base);
	subj = 0;
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		if(subjobj)
		{
			node = librdf_statement_get_subject(st);
		}
		else
		{
			node = librdf_statement_get_object(st);
		}
		if(!librdf_node_is_resource(node))
		{
			librdf_stream_next(stream);
			continue;
		}
		if((uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
/*			st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", uristr);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st); */
			buflen += 8 + (strlen(uristr) * 3);
			p = (char *) realloc(query, buflen);
			if(!p)
			{
				log_printf(LOG_CRIT, "failed to reallocate buffer to %lu bytes\n", (unsigned long) buflen);
				return 500;
			}
			query = p;
			p = strchr(query, 0);
			strcpy(p, "?s = <");
			for(p = strchr(query, 0); *uristr; uristr++)
			{
				if(*uristr == '>')
				{
					*p = '%';
					p++;
					*p = '3';
					p++;
					*p = 'e';
				}
				else
				{
					*p = *uristr;
				}
				p++;
			}
			*p = 0;
			strcpy(p, "> ||");
			subj = 1;
		}
		librdf_stream_next(stream);
	}
	if(subj)
	{
		strcpy(p, "> ) } }");
		if(quilt_sparql_query_rdf(query, request->model))
		{
			log_printf(LOG_ERR, "failed to create model from query\n");
			free(query);
			return 500;
		}
	}
	free(query);
	return 0;
}

static int
coref_lookup(QUILTREQ *request, const char *target)
{
	SPARQL *sparql;
	SPARQLRES *res;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	size_t l;
	char *buf;

	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}	
	res = sparql_queryf(sparql, "SELECT ?s\n"
						"WHERE {\n"
						" GRAPH %V {\n"
						"  <%s> <http://www.w3.org/2002/07/owl#sameAs> ?s .\n"
						" }\n"
						"}\n",
						request->basegraph, target);
	if(!res)
	{
		log_printf(LOG_ERR, "SPARQL query for coreference failed\n");
		return 500;
	}
	row = sparqlres_next(res);
	if(!row)
	{
		sparqlres_destroy(res);
		return 404;
	}
	node = sparqlrow_binding(row, 0);
	if(!node)
	{
		sparqlres_destroy(res);
		return 404;
	}
	if(!librdf_node_is_resource(node) ||
	   !(uri = librdf_node_get_uri(node)) ||
	   !(uristr = (const char *) librdf_uri_as_string(uri)))
	{
		sparqlres_destroy(res);
		return 404;
	}
	l = strlen(request->base);
	if(!strncmp(uristr, request->base, l))
	{
		buf = (char *) malloc(strlen(uristr) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uristr[l]));
	}
	else
	{
		buf = strdup(uristr);
	}
	sparqlres_destroy(res);
	FCGX_FPrintF(request->fcgi->out, "Status: 302 Moved\n"
				 "Server: Quilt\n"
				 "Location: %s\n"
				 "\n", buf);
	free(buf);
	return 200;
}

static int
coref_home(QUILTREQ *request)
{
	librdf_statement *st;
	size_t c;

	const char *uri;

	uri = FCGX_GetParam("uri", request->query);
	if(uri)
	{
		return coref_lookup(request, uri);
	}

	/* Add all of the indices as void:Datasets */
	for(c = 0; indices[c].uri; c++)
	{		
		st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", indices[c].uri);
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		st = quilt_st_create_literal(indices[c].uri, "http://www.w3.org/2000/01/rdf-schema#label", indices[c].title, "en");
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		st = quilt_st_create_uri(indices[c].uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);

		if(indices[c].qclass)
		{
			st = quilt_st_create_uri(indices[c].uri, "http://rdfs.org/ns/void#class", indices[c].qclass);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st);
		}
	}
	/* Return 0, rather than 200, to auto-serialise the model */
	return 0;
}

static int
coref_item(QUILTREQ *request)
{
	char *query;
	librdf_node *g;
	librdf_stream *stream;
	librdf_world *world;

	query = (char *) malloc(strlen(request->subject) + 1024);
	if(!query)
	{
		log_printf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 128);
		return 500;
	}
	sprintf(query, "SELECT DISTINCT * WHERE {\n"
			"GRAPH ?g {\n"
			"  ?s ?p ?o . \n"
			"  FILTER( ?g = <%s> )\n"
			"}\n"
			"}", request->subject);
	if(quilt_sparql_query_rdf(query, request->model))
	{
		log_printf(LOG_ERR, "failed to create model from query\n");
		free(query);
		return 500;
	}
	/* If the model is completely empty, consider the graph to be Not Found */
	if(quilt_model_isempty(request->model))
	{
		free(query);
		return 404;
	}
	free(query);
	/* Find all of the objects of statements in our graph */
	world = quilt_librdf_world();
	g = librdf_new_node_from_uri_string(world, (const unsigned char *) request->subject);
	stream = librdf_model_context_as_stream(request->model, g);
	coref_index_metadata_stream(request, stream, 0);
	librdf_free_stream(stream);
	librdf_free_node(g);
	/* Return 0, rather than 200, to auto-serialise the model */
	return 0;
}
