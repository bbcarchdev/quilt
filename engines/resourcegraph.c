/* resourcegraph: A simple engine which translates the request-URI to a
 * local graph URI, and retrieves all of the triples in that graph.
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

#include "p_resourcegraph.h"

static int resourcegraph_process(QUILTREQ *request);

int
quilt_plugin_init(void)
{
	if(quilt_plugin_register_engine(QUILT_PLUGIN_NAME, resourcegraph_process))
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to register engine\n");
		return -1;
	}
	return 0;
}

static int
resourcegraph_process(QUILTREQ *request)
{
	char *query;
	
	query = (char *) malloc(strlen(request->subject) + 64);
	if(!query)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 64);
		return 500;
	}
	sprintf(query, "SELECT * WHERE { GRAPH <%s> { ?s ?p ?o } }", request->subject);	
	if(quilt_sparql_query_rdf(query, request->model))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
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
