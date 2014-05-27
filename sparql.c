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

static SPARQL *sparql;

int
quilt_sparql_init(void)
{
	char *buf;
	librdf_world *world;
	
	world = quilt_librdf_world();
	if(!world)
	{
		return -1;
	}
	sparql = sparql_create(NULL);
	if(!sparql)
	{
		log_printf(LOG_CRIT, "failed to create SPARQL query object\n");
		return -1;
	}
	buf = config_geta("sparql:query", NULL);	
	sparql_set_query_uri(sparql, buf);
	free(buf);
	sparql_set_world(sparql, world);
	sparql_set_logger(sparql, log_vprintf);
	sparql_set_verbose(sparql, config_get_int("sparql:verbose", 1));
	return 0;
}

/* Perform a SPARQL query: the variables ?s, ?p, and ?o will be mapped to
 * triples, with the optional ?g being mapped to the context.
 */
int
quilt_sparql_query_rdf(const char *query, librdf_model *model)
{
	if(sparql_query_model(sparql, query, strlen(query), model))
	{
		return -1;
	}
	return 0;
}
