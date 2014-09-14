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

/* This is a simple engine which retrieves the contents of the graph
 * named by the request URI.
 */
int
quilt_engine_resourcegraph_process(QUILTREQ *request)
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
	/* Returning 0 (rather than 200) causes the model to be serialized
	 * automatically.
	 */
	return 0;
}
