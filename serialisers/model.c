/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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
#include <jansson.h>
#include <libquilt.h>
#include "model.h"

/* Populate the template data with information from the RDF model.
 *
 * The following dictionary members may be set:
 *
 * 'data': a hash (where the subject URI is the key) of all of the data in
 *    the model.
 * 'results': an ordered array of Result items.
 * 'abstractUri': the URI of the abstract document
 * 'abstract': copied from data[abstractUri]
 * 'primaryTopicUri': the URI of the primary topic in the model
 * 'primaryTopic': the primary topic of the model, copied from
 *    data[primaryTopicUri]
 * 'object': an alias for primaryTopic
 * 'title': if known, the title/label of 'primaryTopic'
 */
int
html_add_model(json_t *dict, QUILTREQ *req)
{
	json_t *items, *item, *primaryTopic, *results, *k;
	librdf_model *model;
	char *abstractUri, *primaryTopicUri;

	primaryTopic = NULL;
	model = quilt_request_model(req);
	items = html_model_items_(req, model);
	/* Locate the 'abstract document URI' and data */
	abstractUri = html_model_abstract_(req, model, items, &item);
	if(abstractUri)
	{
		json_object_set_new(dict, "abstractUri", json_string(abstractUri));
		if(item)
		{
			json_object_set(dict, "abstract", item);
			if((k = json_object_get(item, "title")))
			{
				json_object_set(dict, "title", k);
			}
		}
	}
	/* Locate the primary topic of the document */
	primaryTopicUri = html_model_primaryTopic_(req, model, items, abstractUri, &item);
	if(primaryTopicUri)
	{
		json_object_set_new(dict, "primaryTopicUri", json_string(primaryTopicUri));
		if(item)
		{
			primaryTopic = item;
			json_object_set(dict, "primaryTopic", item);
			json_object_set(dict, "object", item);
			if((k = json_object_get(item, "title")))
			{
				json_object_set(dict, "title", k);
			}
		}
		results = html_model_results_(req, items);
	}
	json_object_set_new(dict, "results", results);
	json_object_set_new(dict, "data", items);
	free(abstractUri);
	free(primaryTopicUri);
	return 0;
}

/* Determine the abstract document URI, and if available return the data
 * relating to it from the 'items' dictionary.
 *
 * If an item was found, the 'abstract' member of it is set to true
 */
static char *
html_model_abstract_(QUILTREQ *req, librdf_model *model, json_t *items, json_t **item)
{
	QUILTCANON *reqcanon;
	char *uri;

	(void) model;

	*item = NULL;
	reqcanon = quilt_request_canonical(req);
	uri = quilt_canon_str(reqcanon, (quilt_request_ext(req) ? QCO_ABSTRACT : QCO_REQUEST));
	*item = json_object_get(items, uri);
	if(*item)
	{	
		json_object_set_new(*item, "abstract", json_true());
	}
	return uri;
}

/* Locate the primary topic of the model and return its URI; attempt to locate
 * the data about it in the 'items' dictionary.
 *
 * If located, the entry in the dictionary will the member 'me' set to true.
 */
static char *
html_model_primaryTopic_(QUILTREQ *req, librdf_model *model, json_t *items, const char *abstract, json_t **item)
{
	QUILTCANON *reqcanon;
	char *uri;

	(void) model;
	(void) abstract;

	*item = NULL;
	reqcanon = quilt_request_canonical(req);
	uri = quilt_canon_str(reqcanon, QCO_NOEXT|QCO_FRAGMENT);
	*item = json_object_get(items, uri);
	if(*item)
	{	
		json_object_set_new(*item, "me", json_true());
	}
	return uri;
}

/* Create an object based upon the content of the model, organised by subject.
 *
 * The resulting object is substituted into the template with the name 'data',
 * and takes the following form:

	data[subject] = {
		subject: 'http://...',
		subjectLink: '<a href="...">...</a>',
		classLabel: 'Thing',
		classSuffix: '(Thing)',
		props: {
			'@': [
				{
				    predicate: '...',
				    value: 'abc123',
				    htmlValue: '<a href="...">abc123</a>',
				    type: 'literal',
				    datatype: null,
				    language: null
				}
			]
		}
	}
 */
static json_t *
html_model_items_(QUILTREQ *req, librdf_model *model)
{
	json_t *items, *item, *props, *prop, *value;
	librdf_world *world;
	librdf_statement *query, *statement;
	librdf_stream *st;
	librdf_node *subj, *pred, *obj;
	const char *uri;

	world = quilt_librdf_world();
	items = json_object();
	query = librdf_new_statement(world);
	st = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(st))
	{
		statement = librdf_stream_get_object(st);
		subj = librdf_statement_get_subject(statement);
		pred = librdf_statement_get_predicate(statement);
		obj = librdf_statement_get_object(statement);
		if(librdf_node_is_resource(subj) && librdf_node_is_resource(pred))
		{
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(subj));
			item = json_object_get(items, uri);
			if(item)
			{
				/* An entry for this subject URI already exists */
				props = json_object_get(item, "props");
			}
			else
			{
				/* Populate a new item structure */
				item = json_object();
				json_object_set_new(items, uri, item);
				json_object_set_new(item, "me", json_false());
				json_object_set_new(item, "slot", json_false());
				json_object_set_new(item, "result", json_false());
				json_object_set_new(item, "abstract", json_false());
				html_model_subject_(req, item, model, subj, uri);
				props = json_object();
				json_object_set_new(item, "props", props);
			}

			/* Obtain the array of values for this predicate URI */
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(pred));
			prop = json_object_get(props, uri);
			if(!prop)
			{
				/* This is the first instance of this predicate that we've
				 * seen for this object; create a new array to hold the
				 * values */
				prop = json_array();
				json_object_set_new(props, uri, prop);
			}
			/* Add a new 'value' structure to the array of values (prop) */
			value = json_object();
			json_array_append_new(prop, value);
			html_model_predicate_(req, value, pred, uri);
			html_model_object_(req, value, obj);
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	librdf_free_statement(query);
	return items;
}

/* Add the details of a specific subject to an 'item' structure which is
 * passed into the template.
 */
static int
html_model_subject_(QUILTREQ *req, json_t *item, librdf_model *model, librdf_node *subject, const char *uri)
{
	double lon, lat;
	json_t *sub;
	struct class_struct *c;
	char *buf, *str;
	URI *uriobj;
	URI_INFO *info;

	uriobj = uri_create_str(uri, NULL);
	if(!uriobj)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to parse subject URI <%s>\n", uri);
		return -1;
	}
	info = uri_info(uriobj);

	json_object_set_new(item, "subject", json_string(uri));
	if(!strncmp(uri, html_baseuri, html_baseurilen))
	{
		buf = (char *) malloc(strlen(uri) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uri[html_baseurilen]));
		json_object_set_new(item, "link", json_string(buf));
		json_object_set_new(item, "uri", json_string(buf));
	}
	else
	{
		buf = quilt_uri_contract(uri);
		json_object_set_new(item, "link", json_string(uri));
		json_object_set_new(item, "uri", json_string(buf));
	}
	c = html_class_match(model, subject);
	str = get_title(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "hasTitle", json_true());
		json_object_set_new(item, "title", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "hasTitle", json_false());
		json_object_set_new(item, "title", json_string(buf));
	}
	str = get_shortdesc(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "shortdesc", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "shortdesc", json_string(""));
	}
	str = get_longdesc(req, model, subject);
	if(str)
	{
		json_object_set_new(item, "description", json_string(str));
		free(str);
	}
	else
	{
		json_object_set_new(item, "description", json_string(""));
	}
	if(buf[0] == '/' || !info->host)
	{
		json_object_set_new(item, "from", json_string(""));
	}
	else
	{
		strcpy(buf, "from ");
		strcpy(&(buf[5]), info->host);
		json_object_set_new(item, "from", json_string(buf));
	}		
	uri_info_destroy(info);
	free(buf);
	if(c)
	{
		json_object_set_new(item, "class", json_string(c->cssClass));
		json_object_set_new(item, "classLabel", json_string(c->label));
		json_object_set_new(item, "classSuffix", json_string(c->suffix));
		json_object_set_new(item, "classDefinite", json_string(c->definite));
	}
	else
	{
		json_object_set_new(item, "class", json_string(""));
		json_object_set_new(item, "classSuffix", json_string(""));
	}

	if(quilt_model_find_double(model, uri, NS_GEO "long", &lon) == 1 &&
	   quilt_model_find_double(model, uri, NS_GEO "lat", &lat) == 1)
	{
		sub = json_object();
		json_object_set_new(sub, "long", json_real(lon));
		json_object_set_new(sub, "lat", json_real(lat));
		json_object_set_new(item, "geo", sub);
	}
	return 0;
}

/* Add the details of a specific predicate to a 'value' structure which is
 * passed into the template.
 */
static int
html_model_predicate_(QUILTREQ *req, json_t *value, librdf_node *predicate, const char *uri)
{
	char *contracted;

	(void) req;
	(void) predicate;

	json_object_set_new(value, "predicateUri", json_string(uri));
	contracted = quilt_uri_contract(uri);
	json_object_set_new(value, "predicateUriLabel", json_string(contracted));
	free(contracted);
	return 0;
}

/* Add the details of a triple's object to a 'value' structure which is
 * passed into the template
 */
static int
html_model_object_(QUILTREQ *req, json_t *value, librdf_node *object)
{
	const char *str, *dtstr, *lang;
	char *buf;
	librdf_uri *dt;

	(void) req;

	if(librdf_node_is_resource(object))
	{
		str = (const char *) librdf_uri_as_string(librdf_node_get_uri(object));
		json_object_set_new(value, "type", json_string("uri"));
		json_object_set_new(value, "isUri", json_true());
		json_object_set_new(value, "value", json_string(str));
		if(!strncmp(str, html_baseuri, html_baseurilen))
		{
			buf = (char *) malloc(strlen(str) + 32);
			buf[0] = '/';
			strcpy(&(buf[1]), &(str[html_baseurilen]));
			json_object_set_new(value, "link", json_string(buf));
			json_object_set_new(value, "uri", json_string(buf));
		}
		else
		{
			buf = quilt_uri_contract(str);
			json_object_set_new(value, "uri", json_string(buf));
			json_object_set_new(value, "link", json_string(str));
		}
		free(buf);
	}
	else if(librdf_node_is_literal(object))
	{
		str = (const char *) librdf_node_get_literal_value(object);

		json_object_set_new(value, "type", json_string("literal"));
		json_object_set_new(value, "isLiteral", json_true());
		json_object_set_new(value, "value", json_string(str));

		lang = librdf_node_get_literal_value_language(object);
		if(lang)
		{
			json_object_set_new(value, "lang", json_string(lang));
		}
		dt = librdf_node_get_literal_value_datatype_uri(object);
		if(dt)
		{
			dtstr = (const char *) librdf_uri_as_string(dt);
			if(dtstr)
			{
				json_object_set_new(value, "datatype", json_string(dtstr));
				buf = quilt_uri_contract(dtstr);
				json_object_set_new(value, "datatypeUri", json_string(buf));
				free(buf);
			}
		}
		else
		{
			dtstr = NULL;
		}
	}
	return 0;
}

/* Generate and return 'results' array, which is an ordered list
 * of slot entries, each of which has the form:
 * [ {
 *	   <result item data>...,
 *	   slot: true/false,
 *	   index: value of olo:index,
 *	   key: value of olo:item,
 *	   item: { <the item object referenced by olo:item> },
 *	 }
 * ]
 *
 * Any objects in 'items' whose classes include olo:Slot have
 * their 'slot' member set to true so that templates can skip them.
 * 
 * Returns:
 *	 json_t *sorted_results
 */
static json_t *
html_model_results_(QUILTREQ *req, json_t *items)
{
	json_t *item, *value, *props;
	const char *key;
	int i;
	int size = MAX(req->limit, req->deflimit);
	json_t* results[size];
	int item_index=0;

	json_object_foreach(items, key, item)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME "key: <%s>\n", key);
		if(html_model_item_is_(item, NS_OLO "Slot"))
		{
			json_object_set_new(item, "slot", json_true());
			props = json_object_get(item, "props");

			/* get the index value and store in item.index */
			json_t *olo_index = json_object_get(props, NS_OLO "index");
			json_array_foreach(olo_index, i, value)
			{
				json_t *index = json_object_get(value, "value");
				if (json_typeof(index) == JSON_STRING)
				{
					json_object_set(item, "index", index);
				}
			}

			/* get the item value and store in item.key */
			json_t *olo_item = json_object_get(props, NS_OLO "item");
			json_array_foreach(olo_item, i, value)
			{
				json_t *item_value = json_object_get(value, "value");
				if (json_typeof(item_value) == JSON_STRING)
				{
					json_object_set(item, "key", item_value);
					json_object_set(item, "item", json_object_get(items, json_string_value(item_value)));
				}
			}

		/* store item in results array for sorting */
		results[item_index++] = item;
		}
	}

	sort_items_by_index(results, item_index);

	/* Convert C array back to json */
	json_t *sorted_items = json_array();
	for (i=0; i<item_index; i++)
	{
		json_array_append(sorted_items, results[i]);
	}

	return sorted_items;
}

/* Compare two items by item.index */
static int
cmp_index(const void *a, const void *b)
{
	json_t *item_a = *(json_t * const *)a;
	json_t *item_b = *(json_t * const *)b;
	char *json_index_a = json_string_value(json_object_get(item_a, "index"));
	char *json_index_b = json_string_value(json_object_get(item_b, "index"));
	long index_a = strtol(json_index_a, NULL, 10);
	long index_b = strtol(json_index_b, NULL, 10);

	return (index_a - index_b);
}


/* Sort an array of result items by the item.index value
 * Args:
 *	json_t *items - json_object of key/value pairs where value is item
 */
int
sort_items_by_index(json_t **items, int size)
{
	qsort(items, size, sizeof(items[0]), cmp_index);
	return(0);
}


/* Determine if the object 'item' has the RDF class 'classuri' */
static int
html_model_item_is_(json_t *item, const char *classuri)
{
	json_t *props, *prop, *value, *k;
	size_t index;

	props = json_object_get(item, "props");
	if(!props)
	{
		return 0;
	}
	prop = json_object_get(props, NS_RDF "type");
	if(!prop)
	{
		return 0;
	}
	json_array_foreach(prop, index, value)
	{
		k = json_object_get(value, "isUri");
		if(!k || json_typeof(k) != JSON_TRUE)
		{
			continue;
		}
		k = json_object_get(value, "value");
		if(!k || json_typeof(k) != JSON_STRING)
		{
			continue;
		}
		if(!strcmp(json_string_value(k), classuri))
		{
			return 1;
		}
	}
	return 0;
}

/* Obtain the title of a particular subject, suitable for substituting into
 * a template. This function should allocate the buffer it returns, or return
 * NULL if no suitable title could be found.
 */
static char *
get_title(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, NS_RDFS "label");
}

/* Obtain the short description of a particular subject, suitable for
 * substituting into a template. This function should allocate the buffer
 * it returns, or return NULL if no suitable title could be found.
 */
static char *
get_shortdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, NS_RDFS "comment");
}

/* Obtain the short description of a particular subject, suitable for
 * substituting into a template. This function should allocate the buffer
 * it returns, or return NULL if no suitable title could be found.
 */
static char *
get_longdesc(QUILTREQ *req, librdf_model *model, librdf_node *subject)
{
	return get_literal(req, model, subject, NS_DCT "description");
}

static char *
get_literal(QUILTREQ *req, librdf_model *model, librdf_node *subject, const char *predicate)
{
	librdf_world *world;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *obj;
	char *specific, *generic, *none;
	/* XXX perform proper language negotiation */
	const char *slang = "en-GB", *glang = "en";
	const char *l, *value;

	(void) req;

	world = quilt_librdf_world();
	specific = NULL;
	generic = NULL;
	none = NULL;
	obj = librdf_new_node_from_uri_string(world, (const unsigned char *) predicate);
	if(!obj)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create new URI for <%s>\n", predicate);
		return NULL;
	}
	query = librdf_new_statement(world);
	if(!query)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create new statement for query\n");
		librdf_free_node(obj);
		return NULL;
	}
	subject = librdf_new_node_from_node(subject);
	if(!subject)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to duplicate subject node\n");
		librdf_free_node(obj);
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, subject);
	librdf_statement_set_predicate(query, obj);
	stream = librdf_model_find_statements(model, query);
	if(!stream)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create stream for model query\n");
		librdf_free_statement(query);
		return NULL;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		obj = librdf_statement_get_object(st);
		if(!obj)
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to obtain object of statement\n");
		}
		else if(librdf_node_is_literal(obj) &&
		   !librdf_node_get_literal_value_datatype_uri(obj))
		{
			l = librdf_node_get_literal_value_language(obj);
			value = (const char *) librdf_node_get_literal_value(obj);
			if(!l)
			{
				if(!generic && !none)
				{
					none = strdup(value);
				}
			}
			else if(!strcasecmp(l, slang))
			{
				specific = strdup(value);
				break;
			}
			else if(!strcasecmp(l, glang) && !generic)
			{
				generic = strdup(value);
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(specific)
	{
		free(generic);
		free(none);
		return specific;
	}
	if(generic)
	{
		free(none);
		return generic;
	}
	return none;
}
