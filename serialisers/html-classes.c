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

#include "p_html.h"

struct class_struct html_classes[] = {
	{
		"http://xmlns.com/foaf/0.1/Person",
		"person",
		"Person",
		"(Person)",
		"a person",
	},
	{
		"http://xmlns.com/foaf/0.1/Group",
		"group",
		"Group",
		"(Group)",
		"a group",
	},
	{
		"http://xmlns.com/foaf/0.1/Agent",
		"agent",
		"Agent",
		"(Agent)",
		"an agent",
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing",
		"place",
		"Place",
		"(Place)",
		"a place",
	},
	{
		"http://www.cidoc-crm.org/cidoc-crm/E18_Physical_Thing",
		"thing",
		"Thing",
		"(Thing)",
		"a physical thing",
	},
	{
		"http://purl.org/dc/dcmitype/Collection",
		"collection",
		"Collection",
		"(Collection)",
		"a collection",
	},
	{
		"http://purl.org/vocab/frbr/core#Work",
		"creative-work",
		"Creative work",
		"(Creative work)",
		"a creative work",
	},
	{
		"http://xmlns.com/foaf/0.1/Document",
		"digital-object",
		"Digital asset",
		"(Digital asset)",
		"a digital asset",
	},
	{
		"http://purl.org/NET/c4dm/event.owl#Event",
		"event",
		"Event",
		"(Event)",
		"an event",
	},
	{
		"http://rdfs.org/ns/void#Dataset",
		"dataset",
		"Dataset",
		"(Dataset)",
		"a dataset",
	},
	{
		"http://www.w3.org/2004/02/skos/core#Concept",
		"concept",
		"Concept",
		"(Concept)",
		"a concept",
	},
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
	}
};

struct class_struct *
html_class_match(librdf_model *model, librdf_node *subject)
{   
	size_t c, score;
	librdf_world *world;
	librdf_stream *st;
	librdf_statement *query, *statement;
	librdf_node *obj;
	const char *uri;
	struct class_struct *match;

	match = NULL;
	score = 100;
	world = quilt_librdf_world();	
	query = librdf_new_statement(world);
	librdf_statement_set_subject(query, librdf_new_node_from_node(subject));
	obj = librdf_new_node_from_uri_string(world, (const unsigned char *) "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	librdf_statement_set_predicate(query, obj);
	st = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(st))
	{
		statement = librdf_stream_get_object(st);
		obj = librdf_statement_get_object(statement);
		if(librdf_node_is_resource(obj))
		{
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(obj));
			if(uri)
			{
				for(c = 0; html_classes[c].uri; c++)
				{
					if(c > score)
					{
						break;
					}
					if(!strcmp(html_classes[c].uri, uri))
					{
						match = &(html_classes[c]);
						score = c;
					}
				}
			}
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	librdf_free_statement(query);
	return match;
}
