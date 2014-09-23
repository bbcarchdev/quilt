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
	{ "/groups", "Groups", "http://xmlns.com/foaf/0.1/Groups" },
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

/* This engine retrieves coreference information as stored by Spindle */
int
quilt_engine_coref_process(QUILTREQ *request)
{
	char *qclass;
	size_t c;
	int r;

	qclass = NULL;
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
	SPARQLROW *row;
	char *query, *p;
	size_t buflen;
	char limofs[128];
	librdf_node *node;
	librdf_uri *uri;
	librdf_statement *st;
	const char *uristr;
	int limit, offset, subj;


	limit = 25;
	offset = 0;
	if(offset)
	{
		snprintf(limofs, sizeof(limofs) - 1, "OFFSET %d LIMIT %d", offset, limit);
	}
	else
	{
		snprintf(limofs, sizeof(limofs) - 1, "LIMIT %d", limit);
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
	sparqlres_destroy(res);
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
	/* Return 0, rather than 200, to auto-serialise the model */
	st = quilt_st_create_literal(request->path, "http://www.w3.org/2000/01/rdf-schema#label", request->indextitle, "en");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	return 0;
}

static int
coref_home(QUILTREQ *request)
{
	librdf_statement *st;
	size_t c;

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
	
	query = (char *) malloc(strlen(request->subject) + 64);
	if(!query)
	{
		log_printf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 64);
		return 500;
	}
	sprintf(query, "SELECT * WHERE { GRAPH <%s> { ?s ?p ?o } }", request->subject);
	if(quilt_sparql_query_rdf(query, request->model))
	{
		log_printf(LOG_ERR, "failed to create model from query\n");
		free(query);
		return 500;
	}
	free(query);
	/* If the model is completely empty, consider the graph to be Not Found */
	if(quilt_model_isempty(request->model))
	{
		return 404;
	}
	/* Return 0, rather than 200, to auto-serialise the model */
	return 0;
}
