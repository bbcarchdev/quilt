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

/* This engine retrieves coreference information as stored by Spindle */
int
quilt_engine_coref_process(QUILTREQ *request)
{
	SPARQL *sparql;
	SPARQLRES *res;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	char *query, *p;
	size_t buflen;
	const char *qclass, *uristr;
	int limit, offset, subj;
	
	qclass = NULL;
	limit = 25;
	offset = 0;
	if(request->home)
	{
		sparql = quilt_sparql();
		if(!sparql)
		{
			return 500;
		}
		res = sparql_queryf(sparql, "SELECT DISTINCT ?s WHERE GRAPH <%s> { ?s ?p ?o . } LIMIT %d", request->base, limit);
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
		sprintf(query, "SELECT ?s ?p ?o ?g WHERE { GRAPH <%s> { ?s ?p ?o .", request->base);
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
		if(!subj)
		{
			return 404;
		}
		p -= 2;
		strcpy(p, ") } }");		
	}
	else
	{
		query = (char *) malloc(strlen(request->subject) + 64);
		if(!query)
		{
			log_printf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 64);
			return 500;
		}
		sprintf(query, "SELECT * WHERE { GRAPH <%s> { ?s ?p ?o } }", request->subject);
	}
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
	/* Returning 0 (rather than 200) causes the model to be serialized
	 * automatically.
	 */
	return 0;
}
