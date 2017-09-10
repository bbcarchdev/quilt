/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015-2017 BBC
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

#include "p_jsonld.h"

/* This plug-in implements JSON-LD serialisation of the RDF model. It
 * attempts to 'do the right thing' automatically as much as possible, using
 * the namespaces defined in the Quilt configuration to generate a context
 * object, and contracting the JSON form wherever possible.
 *
 * Both application/json+ld (extension .jsonld) and application/json (extension
 * .json) are registered by this plug-in with a preference score of 95%.
 *
 * To use this plug-in, add an entry to the [quilt] section of your
 * configuration:
 *
 * [quilt]
 * :
 * :
 * module=jsonld.so
 *
 * Any namespaces defined in the [namespaces] section will be added to the
 * JSON-LD context. Additionally, you can use the [jsonld:aliases] and
 * [jsonld:datatypes] sections to add individual properties to the context.
 *
 * [namespaces]
 * rdfs="http://www.w3.org/2000/01/rdf-schema#"
 *
 * [jsonld:aliases]
 * label=rdfs:label
 * see=rdfs:seeAlso
 *
 * [jsonld:datatypes]
 * see=@id
 *
 * [jsonld:containers]
 * label=@language
 */

typedef struct jsonld_info_struct jsonld_info;

static int jsonld_serialize(QUILTREQ *req);
static int jsonld_serialize_model(jsonld_info *info, librdf_model *model);
static int jsonld_serialize_stream(jsonld_info *info, librdf_node *context, librdf_stream *stream, json_t *set, int recurse);
static int jsonld_serialize_langmaps(jsonld_info *info);

static int jsonld_recurse(jsonld_info *info, json_t *entry, int recurse);
static json_t *jsonld_recurse_value(jsonld_info *info, json_t *value, int recurse);

static json_t *jsonld_subject_locate(jsonld_info *info, json_t *targetarray, json_t *subjectmap, const char *subject, librdf_node *node);
static int jsonld_subject_add_node(jsonld_info *info, json_t *entry, const char *subject, const char *predicate, librdf_node *node, int recurse);
static int jsonld_subject_add_value(jsonld_info *info, json_t *entry, const char *subject, const char *predicate, json_t *value, json_t *propentry);

static int jsonld_value_equals(jsonld_info *info, json_t *a, json_t *b);
static int jsonld_value_present(jsonld_info *info, json_t *list, json_t *value);

static int jsonld_context_set(jsonld_info *info, const char *name, const char *uri, const char *datatype, const char *containers);
static json_t *jsonld_context_locate_node(jsonld_info *info, const char *predicate, librdf_node *node, const char **name);
static int jsonld_context_entry_langmap(jsonld_info *info, json_t *entry, const char *lang);
static const char *jsonld_context_entry_datatype(jsonld_info *info, json_t *entry);
static int jsonld_context_entry_container(jsonld_info *info, json_t *entry, const char *type);

static char *jsonld_predicate_locate(jsonld_info *info, const char *predicate, json_t **declarator);

/* RDF node and URI manipulation */
static json_t *jsonld_node(jsonld_info *info, json_t *container, librdf_node *node, const char *preduristr, json_t *propentry, int recurse);
static json_t *jsonld_uri_node(jsonld_info *info, librdf_node *node);
static json_t *jsonld_uri(jsonld_info *info, librdf_uri *uri);
static char *jsonld_uri_contractstr(jsonld_info *info, librdf_uri *uri);
static const char *jsonld_uri_node_relstr(jsonld_info *info, librdf_node *node);
static const char *jsonld_uri_relstr(jsonld_info *info, librdf_uri *uri);
static const char *jsonld_relstr(jsonld_info *info, const char *str);
static char *jsonld_relstr_contract(jsonld_info *info, const char *str);

/* quilt_config_get_all() callbacks */
static int jsonld_ns_cb(const char *key, const char *value, void *data);
static int jsonld_aliases_cb(const char *key, const char *value, void *data);
static int jsonld_datatypes_cb(const char *key, const char *value, void *data);
static int jsonld_containers_cb(const char *key, const char *value, void *data);

static QUILTTYPE jsonld_types[] = {
	{ "application/ld+json", "jsonld", "JSON-LD", 0.95f, 1, NULL },
	{ "application/json", "json", "JSON", 0.95f, 0, NULL },
	{ NULL, NULL, NULL, 0, 0, NULL },
};

static int nographs = 1;
static int subjectonly = 1;

struct jsonld_info_struct
{
	/* The request */
	QUILTREQ *req;
	/* The model */
	librdf_model *model;
	/* The Content-Location */
	char *location;
	/* The primary topic */
	const char *subject;
	/* The default graph */
	const char *defgraph;
	/* The base graph */
	const char *basegraph;
	size_t basegraphlen;
	/* The root of the document */
	json_t *root;
	/* The context object */
	json_t *context;
	/* The default graph @set */
	json_t *rootset;
	/* The named graphs array */
	json_t *graphs;
	/* Helper object for language-mapping */
	json_t *langmaps;
	/* The set of subjects in the current graph */
	json_t *kvset;
};

int
quilt_plugin_init(void)
{
	size_t c;

	for(c = 0; jsonld_types[c].mimetype; c++)
	{
		quilt_plugin_register_serializer(&(jsonld_types[c]), jsonld_serialize);
	}
	return 0;
}

static int
jsonld_serialize(QUILTREQ *req)
{
	jsonld_info info;
	char *buf;
	QUILTCANOPTS opt = QCO_CONCRETE|QCO_NOABSOLUTE;

	memset(&info, 0, sizeof(jsonld_info));
	info.req = req;
	if(quilt_request_status(req) > 299)
	{
		// During error, we might need to use user supplied path, SPINDLE#66
		opt |= QCO_USERSUPPLIED;
	}
	info.basegraph = quilt_request_base();
	if(info.basegraph)
	{
		quilt_logf(LOG_DEBUG, "jsonld: base graph is <%s>\n", info.basegraph);
		info.basegraphlen = strlen(info.basegraph);
	}
	info.location = quilt_canon_str(quilt_request_canonical(req), opt);
	quilt_logf(LOG_DEBUG, "jsonld: location is <%s>\n", info.location);
	info.defgraph = quilt_request_graph_uristr(req);
	quilt_logf(LOG_DEBUG, "jsonld: default graph is <%s>\n", info.defgraph);
	info.subject = quilt_request_subject(req);
	quilt_logf(LOG_DEBUG, "jsonld: subject is <%s>\n", info.subject);

	/* root */
	info.root = json_object();
	
	/* root/@context */
	info.context = json_object();
	if(info.basegraph)
	{
		/* root/@context/@base */
		json_object_set_new(info.context, "@base", json_string(info.basegraph));
	}
	/* Add all of the configured namespaces, aliases, and datatypes to the context */
	quilt_config_get_all("namespaces", NULL, jsonld_ns_cb, (void *) &info);
	quilt_config_get_all("jsonld:aliases", NULL, jsonld_aliases_cb, (void *) &info);
	quilt_config_get_all("jsonld:datatypes", NULL, jsonld_datatypes_cb, (void *) &info);
	quilt_config_get_all("jsonld:containers", NULL, jsonld_containers_cb, (void *) &info);

	/* root/@set */
	info.rootset = json_array();

	/* root/@graph */
	info.graphs = json_array();

	info.model = quilt_request_model(req);
	jsonld_serialize_model(&info, info.model);

	json_object_set(info.root, "@context", info.context);

	buf = json_dumps(info.root, JSON_PRESERVE_ORDER|JSON_ENCODE_ANY|JSON_INDENT(2));

	quilt_request_headerf(req, "Status: %d %s\n", quilt_request_status(req), quilt_request_statustitle(req));
	quilt_request_headerf(req, "Content-Type: %s\n", req->type);
	quilt_request_headerf(req, "Content-Location: %s\n", info.location);
	quilt_request_headers(req, "Vary: Accept\n");
	quilt_request_headers(req, "Server: " PACKAGE_SIGNATURE "\n");

	json_decref(info.graphs);
	json_decref(info.rootset);
	json_decref(info.context);
	json_decref(info.root);
	free(info.location);

	if(!buf)
	{
		quilt_request_puts(req, "{\"@error\":\"failed to serialize JSON buffer\"}\n");
	}
	else
	{
		quilt_request_puts(req, buf);
	}
	free(buf);
	return 0;
}

static int
jsonld_serialize_model(jsonld_info *info, librdf_model *model)
{
	char *subjstr;
	librdf_stream *stream;
	librdf_iterator *iter;
	librdf_node *context;
	json_t *set, *graph, *entry;
	size_t n;
	int recurse;

	subjstr = NULL;
	recurse = 0;
	if(subjectonly)
	{
		subjstr = jsonld_relstr_contract(info, jsonld_relstr(info, info->subject));
		quilt_logf(LOG_DEBUG, "jsonld: root node is <%s>\n", subjstr);
		recurse = 8;
	}
	iter = librdf_model_get_contexts(model);
	if(!iter || librdf_iterator_end(iter))
	{
		quilt_logf(LOG_DEBUG, "jsonld: serialising default graph\n");
		info->langmaps = json_object();
		info->kvset = json_object();
		stream = librdf_model_as_stream(model);
		jsonld_serialize_stream(info, NULL, stream, info->rootset, recurse);
		librdf_free_stream(stream);
	}
	for(; iter && !librdf_iterator_end(iter); librdf_iterator_next(iter))
	{
		context = librdf_iterator_get_object(iter);
		quilt_logf(LOG_DEBUG, "jsonld: serialising graph <%s>\n", jsonld_uri_node_relstr(info, context));
		if(nographs)
		{
			/* If context is the default graph, serialise into
			 * the root set.
			 */
			if(!info->langmaps)
			{
				info->langmaps = json_object();
			}
			if(!info->kvset)
			{
				info->kvset = json_object();
			}
			stream = librdf_model_context_as_stream(model, context);
			jsonld_serialize_stream(info, context, stream, info->rootset, recurse);
			librdf_free_stream(stream);
			continue;
		}	   
		if(info->langmaps)
		{
			json_decref(info->langmaps);
		}
		if(info->kvset)
		{
			json_decref(info->kvset);
		}
		info->langmaps = json_object();
		info->kvset = json_object();
		graph = json_object();
		json_object_set_new(graph, "@id", jsonld_uri_node(info, context));
		
		set = json_array();
		
		stream = librdf_model_context_as_stream(model, context);
		jsonld_serialize_stream(info, context, stream, set, recurse);
		jsonld_serialize_langmaps(info);
		librdf_free_stream(stream);
		
		if(json_array_size(set))
		{
			json_object_set(graph, "@graph", set);
			json_array_append(info->graphs, graph);
		}
		
		json_decref(set);
		json_decref(graph);
	}
	if(nographs && info->langmaps && info->kvset)
	{
		jsonld_serialize_langmaps(info);
	}
	if(info->langmaps)
	{
		json_decref(info->langmaps);
		info->langmaps = NULL;
	}
	if(subjstr)
	{
		/* Replace the rootset with a new containing only the subject
		 * and anything directly connected to it by following each
		 * objects @idprops property
		 */
		json_decref(info->rootset);
		info->rootset = json_array();
		entry = json_object_get(info->kvset, subjstr);
		if(entry)
		{
			json_array_append(info->rootset, entry);
			jsonld_recurse(info, entry, recurse);
		}
	}
	if(info->kvset)
	{
		json_decref(info->kvset);
		info->kvset = NULL;
	}
	if(iter)
	{
		librdf_free_iterator(iter);
	}
	if((n = json_array_size(info->rootset)))
	{
		if(n == 1)
		{
			json_object_update(info->root, json_array_get(info->rootset, 0));
		}
		else
		{
			json_object_set(info->root, "@set", info->rootset);
		}
	}
	else if(json_array_size(info->graphs))
	{
		json_object_set(info->root, "@graph", info->graphs);
	}
	free(subjstr);
	return 0;
}

static int
jsonld_serialize_stream(jsonld_info *info, librdf_node *context, librdf_stream *stream, json_t *targetarray, int recurse)
{
	librdf_node *prevsubj, *subject, *predicate, *object;
	librdf_uri *preduri;
	const char *subjuristr;
	char *preduristr;
	librdf_statement *statement;
	json_t *entry, *prop;
	size_t index;

	(void) context;

	prevsubj = NULL;
	entry = NULL;
	for(; !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		statement = librdf_stream_get_object(stream);
		subject = librdf_statement_get_subject(statement);
		subjuristr = jsonld_uri_node_relstr(info, subject);
		predicate = librdf_statement_get_predicate(statement);
		preduri = librdf_node_get_uri(predicate);
		preduristr = jsonld_uri_contractstr(info, preduri);
		object = librdf_statement_get_object(statement);
		if(prevsubj != subject)
		{			
			if(entry)
			{
				json_decref(entry);
			}
			entry = jsonld_subject_locate(info, targetarray, info->kvset, subjuristr, subject);
			if(!entry)
			{
				quilt_logf(LOG_ERR, "failed to create or locate subject <%s> within our target\n");
				continue;
			}
		}
		prevsubj = subject;
		jsonld_subject_add_node(info, entry, subjuristr, preduristr, object, recurse - 1);
		free(preduristr);
	}
	if(entry)
	{
		json_decref(entry);
	}
	/* Remove any null @type keys */
	json_array_foreach(targetarray, index, entry)
	{
		if(json_typeof(entry) != JSON_OBJECT)
		{
			continue;
		}
		prop = json_object_get(entry, "@type");
		if(prop && json_typeof(prop) == JSON_NULL)
		{
			json_object_del(entry, "@type");
		}
	}
	return 0;
}

static int
jsonld_recurse(jsonld_info *info, json_t *entry, int recurse)
{
	json_t *idprops, *dummy, *obj, *newval;
	const char *key;

	if(recurse < 1)
	{
		return 0;
	}
	idprops = json_object_get(entry, "@idprops");
	if(!idprops)
	{
		return 0;
	}
	json_incref(idprops);
	json_object_del(entry, "@idprops");
	json_object_foreach(idprops, key, dummy)
	{
		obj = json_object_get(entry, key);
		newval = jsonld_recurse_value(info, obj, recurse);
		json_object_set(entry, key, newval);
	}
	return 0;
}

static json_t *
jsonld_recurse_value(jsonld_info *info, json_t *value, int recurse)
{
	json_t *id, *obj, *newvalues, *newval;
	size_t idx;

	if(json_typeof(value) == JSON_ARRAY)
	{
		newvalues = json_array();
		json_array_foreach(value, idx, obj)
		{
			newval = jsonld_recurse_value(info, obj, recurse);
			json_array_append(newvalues, newval);
		}
		return newvalues;
	}
	else if(json_typeof(value) == JSON_STRING)
	{
		obj = json_object_get(info->kvset, json_string_value(value));
		if(obj)
		{
			json_incref(obj);
			jsonld_recurse(info, obj, recurse - 1);
			return obj;
		}
	}
	else if(json_typeof(value) == JSON_OBJECT)
	{
		id = json_object_get(value, "@id");
		if(id && json_typeof(id) == JSON_STRING)
		{
			obj = json_object_get(info->kvset, json_string_value(id));
			if(obj)
			{
				json_incref(obj);
				jsonld_recurse(info, obj, recurse - 1);
				return obj;
			}
		}
	}
	return value;
}

/* Merge the language-maps into the output now that they've been fully
 * assembled
 */
static int
jsonld_serialize_langmaps(jsonld_info *info)
{
	void *subjiter, *propiter;
	const char *subjuristr, *uristr;
	json_t *entry, *props;

	for(subjiter = json_object_iter(info->langmaps); subjiter; subjiter = json_object_iter_next(info->langmaps, subjiter))
	{
		subjuristr = json_object_iter_key(subjiter);
		entry = json_object_get(info->kvset, subjuristr);
		if(!entry)
		{
			quilt_logf(LOG_WARNING, "jsonld: while merging language-maps, subject <%s> does not exist in subject map\n", subjuristr);
			continue;
		}
		props = json_object_iter_value(subjiter);
		for(propiter = json_object_iter(props); propiter; propiter = json_object_iter_next(props, propiter))
		{
			uristr = json_object_iter_key(propiter);
			jsonld_subject_add_value(info, entry, subjuristr, uristr, json_object_iter_value(propiter), NULL);
		}
	}
	return 0;
}

/* Helper used to locate (creating if needed) an entry for a subject within a
 * target @set
 */
static json_t *
jsonld_subject_locate(jsonld_info *info, json_t *targetarray, json_t *subjectmap, const char *subject, librdf_node *node)
{
	json_t *entry;

	/* If the entry already exists within our subject map,
	 * just return it
	 */
	if((entry = json_object_get(subjectmap, subject)))
	{
		/* json_object_get() borrows the retained reference,
		 * so we incref here so our caller's subsequent decref
		 * doesn't do the wrong thing
		 */
		json_incref(entry);
		return entry;
	}
	/* We haven't encountered this subject within the graph yet */
	entry = json_object();
	json_object_set_new(entry, "@id", jsonld_uri_node(info, node));
	json_object_set_new(entry, "@type", json_null());
	json_object_set(subjectmap, subject, entry);
	json_array_append(targetarray, entry);	
	return entry;
}

/* Helper used to add a property value to a subject */
static int
jsonld_subject_add_node(jsonld_info *info, json_t *entry, const char *subject, const char *predicate, librdf_node *node, int recurse)
{
	json_t *propentry;
	const char *propname;

	json_t *value, *langprops, *langentry;
	const char *lang;

	propentry = jsonld_context_locate_node(info, predicate, node, &propname);
	if(propentry)
	{
		if(propname)
		{
			predicate = propname;
		}
		/* Should we use one of the abbreviated forms? */
		if(!librdf_node_get_literal_value_datatype_uri(node))
		{
			lang = librdf_node_get_literal_value_language(node);
			if(jsonld_context_entry_langmap(info, propentry, lang))
			{
				/* Express this value as part of a language map */
				langprops = json_object_get(info->langmaps, subject);
				if(!langprops)
				{
					langprops = json_object();
					json_object_set_new(info->langmaps, subject, langprops);
					/* implicitly borrow the reference */
				}
				langentry = json_object_get(langprops, predicate);
				if(!langentry)
				{
					langentry = json_object();
					json_object_set_new(langprops, predicate, langentry);
					/* implicitly borrow the reference */
				}
				json_object_set_new(langentry, lang, json_string((const char *) librdf_node_get_literal_value(node)));
				return 0;
			}
		}
	}
	value = jsonld_node(info, entry, node, predicate, propentry, recurse);
	jsonld_subject_add_value(info, entry, subject, predicate, value, propentry);
	json_decref(value);
	return 0;
}

/* Helper used to add a property value to a subject */
static int
jsonld_subject_add_value(jsonld_info *info, json_t *entry, const char *subject, const char *predicate, json_t *value, json_t *propentry)
{
	json_t *prop, *array;

	(void) info;
	(void) subject;

	prop = json_object_get(entry, predicate);
	if(prop && !strcmp(predicate, "@type") && json_typeof(prop) == JSON_NULL)
	{
		/* If this is the first time we're expressing
		 * rdf:type, there's a NULL placeholder that we should
		 * replace.
		 */
		json_object_set(entry, predicate, value);
		return 0;
	}
	if(prop && json_typeof(prop) != JSON_ARRAY)
	{
		/* A single value for this property exists; convert it to an array
		 * and then append the new value to it
		 */
		if(!jsonld_value_equals(info, prop, value))
		{
			json_incref(prop);
			array = json_array();
			json_array_append(array, prop);
			json_object_set(entry, predicate, array);
			json_decref(prop);
			/* Borrow the reference, as json_object_get() does */
			prop = array;
			json_decref(array);
			/* Append the new value */
			json_array_append(prop, value);
		}
		return 0;
	}
	if(prop)
	{
		/* The property exists and is an array, simply append */
		if(!jsonld_value_present(info, prop, value))
		{
			json_array_append(prop, value);
		}
		return 0;
	}
	if(jsonld_context_entry_container(info, propentry, "@list") ||
	   jsonld_context_entry_container(info, propentry, "@set"))
	{
		/* The context specifies that we should always express the property as
		 * an array
		 */
		array = json_array();
		json_array_append(prop, value);
		json_object_set(entry, predicate, array);
		json_decref(array);
		return 0;
	}
	/* This is the first time we've encountered this property, simply
	 * set it; if there are multiple values, the code above will wrap
	 * this first value in an array when the second relevant triple is
	 * encountered.
	 */
	json_object_set(entry, predicate, value);
	return 0;
}

/* Compare two property values and return nonzero if they're equal */
static int
jsonld_value_equals(jsonld_info *info, json_t *a, json_t *b)
{
	(void) info;

	return json_equal(a, b);
}

/* Determine whether a value is already present in a property array */
static int
jsonld_value_present(jsonld_info *info, json_t *list, json_t *value)
{
	size_t index;
	json_t *obj;
	int r;

	r = 0;
	json_array_foreach(list, index, obj)
	{
		if(jsonld_value_equals(info, obj, value))
		{
			r = 1;
			break;
		}
	}
	return r;
}

/* Return a JSON object for a node */
static json_t *
jsonld_node(jsonld_info *info, json_t *container, librdf_node *node, const char *preduristr, json_t *propentry, int recurse)
{
	json_t *obj;
	librdf_uri *nodedt;
	const char *dturi, *nodedturistr, *value, *lang;
	json_int_t intval;
	double realval;
	char *endp, *nodetype;
	json_t *idprops;

	endp = NULL;
	if(propentry)
	{
		/* Is a datatype specified in the context for this predicate? */
		dturi = jsonld_context_entry_datatype(info, propentry);
	}
	else if(!strcmp(preduristr, "@type"))
	{
		dturi = "@id";
	}
	else
	{
		dturi = NULL;
	}
	if(librdf_node_is_resource(node))
	{
		if(recurse && container)
		{
			idprops = json_object_get(container, "@idprops");
			if(!idprops)
			{
				idprops = json_object();
				json_object_set_new(container, "@idprops", idprops);
			}
			json_object_set_new(idprops, preduristr, json_true());
		}
		if(dturi && !strcmp(dturi, "@id"))
		{
			/* The datatype is @id - i.e., return a bare URI-string */
			return jsonld_uri_node(info, node);
		}
		obj = json_object();
		json_object_set_new(obj, "@id", jsonld_uri_node(info, node));
		return obj;
	}

	if(librdf_node_is_literal(node))
	{
		/* Serialise a literal, according to the built-in optimal serialisation
		 * rules (e.g., encoding xsd:boolean values) along with the information
		 * in the context.
		 */
		value = (const char *) librdf_node_get_literal_value(node);		
		if((nodedt = librdf_node_get_literal_value_datatype_uri(node)))
		{
			nodedturistr = (const char *) librdf_uri_as_string(nodedt);
			/* If the datatype is xsd:boolean, emit a JSON true or false */
			if(!strcmp(nodedturistr, NS_XSD "boolean"))
			{
				if(!strcmp(value, "true") || !strcmp(value, "1"))
				{
					return json_true();
				}
				if(!strcmp(value, "false") || !strcmp(value, "0"))
				{
					return json_false();
				}
			}
			if(!strcmp(nodedturistr, NS_XSD "decimal") ||
			   !strcmp(nodedturistr, NS_XSD "float") ||
			   !strcmp(nodedturistr, NS_XSD "double"))
			{
				realval = strtod(value, &endp);
				if(!endp || !endp[0])
				{
					return json_real(realval);
				}
			}
			if(!strcmp(nodedturistr, NS_XSD "integer") ||
			   !strcmp(nodedturistr, NS_XSD "long") ||
			   !strcmp(nodedturistr, NS_XSD "unsignedLong") ||
			   !strcmp(nodedturistr, NS_XSD "int") ||
			   !strcmp(nodedturistr, NS_XSD "unsignedInt") ||
			   !strcmp(nodedturistr, NS_XSD "short") ||
			   !strcmp(nodedturistr, NS_XSD "unsignedShort") ||
			   !strcmp(nodedturistr, NS_XSD "byte") ||
			   !strcmp(nodedturistr, NS_XSD "unsignedByte") ||
			   !strcmp(nodedturistr, NS_XSD "nonPositiveInteger") ||
			   !strcmp(nodedturistr, NS_XSD "negativeInteger") ||
			   !strcmp(nodedturistr, NS_XSD "nonNegativeInteger") ||
			   !strcmp(nodedturistr, NS_XSD "positiveInteger"))
			{
				/* Note that we perform minimal checking here; for our
				 * purposes we don't care about the constraints applied
				 * to some of the types (e.g,. nonNegativeInteger) - provided
				 * the value is parseable to an integer of some kind, we
				 * will encode it as a JSON Number -- it's not our role to
				 * validate the RDF.
				 */
#if JSON_INTEGER_IS_LONG_LONG
				intval = strtoll(value, &endp, 10);
#else
				intval = strtol(value, &endp, 10);
#endif
				if(!endp || !endp[0])
				{
					return json_integer(intval);
				}
			}
			/* If the datatype in the context matches the datatype of the node,
			 * return the value as a string
			 *
			 * Note that we still manipulate nodedturistr whether or not
			 * dturi is NULL. That is, there are optional comparisons intermixed
			 * with the URI-string manipulation. This is so that we can compare
			 * with dturi (the context datatype URI) against the node's datatype
			 * URI as it passes through each stage of transformation (full,
			 * host-relative, contracted).
			 */
			if(dturi && !strcmp(dturi, nodedturistr))
			{
				return json_string(value);
			}
			nodedturistr = jsonld_relstr(info, nodedturistr);
			if(dturi && !strcmp(dturi, nodedturistr))
			{
				return json_string(value);
			}
			nodetype = jsonld_relstr_contract(info, nodedturistr);
			if(dturi && !strcmp(dturi, nodetype))
			{
				free(nodetype);
				return json_string(value);
			}
			obj = json_object();
			json_object_set_new(obj, "@value", json_string(value));
			json_object_set_new(obj, "@type", json_string(nodetype));
			free(nodetype);
			return obj;
		}
		if((lang = (const char *) librdf_node_get_literal_value_language(node)))
		{
			obj = json_object();
			json_object_set_new(obj, "@value", json_string(value));
			json_object_set_new(obj, "@language", json_string(lang));
			return obj;
		}
		return json_string(value);
	}
	return json_string("<unsupported node type>");
}

/* Return a JSON string for the (possibly compacted form) of a resource node */
static json_t *
jsonld_uri_node(jsonld_info *info, librdf_node *node)
{
	librdf_uri *uri;
	
	if(!librdf_node_is_resource(node))
	{
		return json_string("<node is not a resource>");
	}
	uri = librdf_node_get_uri(node);
	if(!uri)
	{
		return json_string("<invalid URI>");
	}
	return jsonld_uri(info, uri);
}

/* Return a new JSON string representing the RDF URI provided in the most
 * compact form available
 */
static json_t *
jsonld_uri(jsonld_info *info, librdf_uri *uri)
{
	char *str;
	json_t *r;

	str = jsonld_uri_contractstr(info, uri);
	r = json_string(str);
	free(str);
	return r;
}

/* Return a string representing the RDF URI provided in the
 * most compact form available
 */
static char *
jsonld_uri_contractstr(jsonld_info *info, librdf_uri *uri)
{
	const char *str;

	str = (const char *) librdf_uri_as_string(uri);
	if(!str)
	{
		return strdup("@null");
	}
	if(!strcmp(str, NS_RDF "type"))
	{
		return strdup("@type");
	}
	str = jsonld_relstr(info, str);
	return jsonld_relstr_contract(info, str);
}

/* Given a URI string (which should have been passed through jsonld_relstr())
 * return a copy in the most compact/aliased form
 */
static char *
jsonld_relstr_contract(jsonld_info *info, const char *str)
{
	char *cstr, *name;
	
	cstr = quilt_uri_contract(str);
	name = jsonld_predicate_locate(info, cstr, NULL);
	if(name)
	{
		free(cstr);
		return name;
	}	
	return cstr;
}

/* Return the supplied node (jsonld_uri_node_relstr()) or URI (jsonld_uri_relstr()) as
 * a string, making it host-relative if it's prefixed by the base graph URI
 */
static const char *
jsonld_uri_node_relstr(jsonld_info *info, librdf_node *node)
{
	if(!librdf_node_is_resource(node))
	{
		return NULL;
	}
	return jsonld_uri_relstr(info, librdf_node_get_uri(node));
}

static const char *
jsonld_uri_relstr(jsonld_info *info, librdf_uri *uri)
{
	const char *str;

	str = (const char *) librdf_uri_as_string(uri);
	return jsonld_relstr(info, str);
}

/* If str begins with the base graph URI, skip the matching
 * portion so that the URI is host-relative, otherwise just
 * return str unchanged.
 */
static const char *
jsonld_relstr(jsonld_info *info, const char *str)
{
	if(info->basegraph && info->basegraphlen)
	{
		if(!strncmp(str, info->basegraph, info->basegraphlen))
		{
			/* There will always be a trailing slash */
			str += info->basegraphlen - 1;
		}			
	}
	return str;
}	

char *
jsonld_predicate_locate(jsonld_info *info, const char *predicate, json_t **declarator)
{
	const char *key, *uristr;
	json_t *value, *str;

	json_object_foreach(info->context, key, value)
	{
		if(key[0] == '@' || json_typeof(value) != JSON_OBJECT)
		{
			continue;
		}
		if((str = json_object_get(value, "@id")))
		{
			uristr = json_string_value(str);
		}
		else
		{
			uristr = NULL;
		}
		if(!strcmp(key, predicate) || (uristr && !strcmp(uristr, predicate)))
		{
			if(declarator)
			{
				json_incref(value);
				*declarator = value;
			}
			return strdup(key);
		}
	}
	return NULL;
}

/* Add an entry to a the @context object (if it does not already exist), and set
 * any combination of the URI (@id), datatype (@type), container (@container).
 *
 * This function always adds/updates the entry, never replaces, so it is safe to
 * do the following:
 *
 * jsonld_context_set(info, "topic", "foaf:topic", NULL, NULL);
 * jsonld_context_set(info, "topic", NULL, "@id", NULL);
 * jsonld_context_set(info, "label", "rdfs:label", NULL, "@language");
 *
 * The resulting entry would be:
 *
 *    "topic": { "@id": "foaf:topic", "@type": "@id" }
 */

static int
jsonld_context_set(jsonld_info *info, const char *name, const char *uri, const char *datatype, const char *containers)
{
	json_t *obj;
	char *s;
	int add;

	if((obj = json_object_get(info->context, name)))
	{
		json_incref(obj);
		add = 0;
	}
	else
	{		
		obj = json_object();
		add = 1;
	}
	if(uri)
	{
		s = quilt_uri_contract(jsonld_relstr(info, uri));
		json_object_set_new(obj, "@id", json_string(s));
		free(s);
	}
	if(datatype)
	{
		s = quilt_uri_contract(jsonld_relstr(info, datatype));
		json_object_set_new(obj, "@type", json_string(s));
		free(s);
	}
	if(containers)
	{
		json_object_set_new(obj, "@container", json_string(containers));
	}
	if(add && json_object_size(obj))
	{
		json_object_set(info->context, name, obj);
	}
	json_decref(obj);
	return 0;
}

/* Locate an entry within an @context; the predicate URI should be in contracted form */
static json_t *
jsonld_context_locate_node(jsonld_info *info, const char *predicate, librdf_node *node, const char **name)
{
	const char *key, *entryuristr;
	json_t *value, *entryuri;
	const char *default_key, *lang_key, *datatype_key;
	json_t *default_value, *lang_value, *datatype_value;

	(void) node;

	default_key = lang_key = datatype_key = NULL;
	default_value = lang_value = datatype_value = NULL;

	/* There may be multiple entries within a context with different names
	 * and datatypes or languages, so we need to attempt to find the most
	 * appropriate match for the node we're serialising.
	 */

	json_object_foreach(info->context, key, value)
	{
		if(key[0] == '@' || json_typeof(value) != JSON_OBJECT)
		{
			continue;
		}
		if((entryuri = json_object_get(value, "@id")))
		{
			entryuristr = json_string_value(entryuri);
		}
		else
		{
			entryuristr = NULL;
		}
		if(!strcmp(key, predicate) || (entryuristr && !strcmp(entryuristr, predicate)))
		{
			/* We have a match of some kind */
			default_key = key;
			default_value = value;
		}
	}
	if(datatype_key)
	{
		if(name)
		{
			*name = datatype_key;
		}
		return datatype_value;
	}
	if(lang_key)
	{
		if(name)
		{
			*name = lang_key;
		}
		return lang_value;
	}
	if(name)
	{
		*name = default_key;
	}
	return default_value;
}

/* Should a literal value be expressed as part of a language-map? */
static int
jsonld_context_entry_langmap(jsonld_info *info, json_t *entry, const char *lang)
{
	(void) lang;

	if(jsonld_context_entry_container(info, entry, "@language"))
	{
		return 1;
	}
	return 0;
}

/* Return the datatype, if any, specified in a context entry */
static const char *
jsonld_context_entry_datatype(jsonld_info *info, json_t *entry)
{
	json_t *type;

	(void) info;

	type = json_object_get(entry, "@type");
	if(type)
	{
		return json_string_value(type);
	}
	return NULL;
}

/* Does the context entry specify: @container: <type>? */
static int
jsonld_context_entry_container(jsonld_info *info, json_t *entry, const char *type)
{
	json_t *container, *value;
	size_t index;
	const char *valstr;

	(void) info;

	container = json_object_get(entry, "@container");
	if(!container)
	{
		return 0;
	}
	if(json_typeof(container) == JSON_STRING)
	{
		valstr = json_string_value(container);
		if(valstr && !strcmp(valstr, type))
		{
			return 1;
		}
		return 0;
	}
	if(json_typeof(container) != JSON_ARRAY)
	{
		return 0;
	}
	json_array_foreach(container, index, value)
	{
		valstr = json_string_value(value);
		if(valstr && !strcmp(valstr, type))
		{
			return 1;
		}
	}
	return 0;
}

/* Callbacks for quilt_config_get_all() */

/* Invoked for each key/value pair in the [namespaces] section */
static int
jsonld_ns_cb(const char *key, const char *value, void *data)
{
	jsonld_info *info;

	info = (jsonld_info *) data;
	
	if(strncmp(key, "namespaces:", 11))
	{
		return 0;
	}
	key += 11;
	json_object_set_new(info->context, key, json_string(value));
	return 0;
}

/* Invoked for each key/value pair in the [jsonld:aliases] section */
static int
jsonld_aliases_cb(const char *key, const char *value, void *data)
{
	jsonld_info *info;

	if(strncmp(key, "jsonld:aliases:", 15))
	{
		return 0;
	}
	key += 15; /* jsonld:aliases: */
	info = (jsonld_info *) data;
	jsonld_context_set(info, key, value, NULL, NULL);
	return 0;
}

/* Invoked for each key/value pair in the [jsonld:datatypes] section */
static int
jsonld_datatypes_cb(const char *key, const char *value, void *data)
{
	jsonld_info *info;

	if(strncmp(key, "jsonld:datatypes:", 17))
	{
		return 0;
	}
	key += 17; /* jsonld:datatypes: */
	info = (jsonld_info *) data;
	jsonld_context_set(info, key, NULL, value, NULL);
	return 0;
}

/* Invoked for each key/value pair in the [jsonld:containers] section */
static int
jsonld_containers_cb(const char *key, const char *value, void *data)
{
	jsonld_info *info;

	if(strncmp(key, "jsonld:containers:", 18))
	{
		return 0;
	}
	key += 18; /* jsonld:containers: */
	info = (jsonld_info *) data;
	jsonld_context_set(info, key, NULL, NULL, value);
	return 0;
}
