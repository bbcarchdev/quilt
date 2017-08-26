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
 */

typedef struct jsonld_info_struct jsonld_info;

static int jsonld_serialize(QUILTREQ *req);
static int jsonld_serialize_stream(jsonld_info *info, librdf_node *context, librdf_stream *stream, json_t *set);
static json_t *jsonld_node(jsonld_info *info, librdf_node *node, const char *preduristr);
static json_t *jsonld_uri_node(jsonld_info *info, librdf_node *node);
static json_t *jsonld_uri(jsonld_info *info, librdf_uri *uri);
static char *jsonld_uri_contractstr(jsonld_info *info, librdf_uri *uri);
static const char *jsonld_uri_node_relstr(jsonld_info *info, librdf_node *node);
static const char *jsonld_uri_relstr(jsonld_info *info, librdf_uri *uri);
static const char *jsonld_relstr(jsonld_info *info, const char *str);
static char *jsonld_relstr_contract(jsonld_info *info, const char *str);
static const char *jsonld_predicate_datatype(jsonld_info *info, const char *predicate);
static char *jsonld_predicate_locate(jsonld_info *info, const char *predicate, json_t **declarator);
static int jsonld_context_set(jsonld_info *info, const char *name, const char *uri, const char *datatype);

static int jsonld_ns_cb(const char *key, const char *value, void *data);
static int jsonld_aliases_cb(const char *key, const char *value, void *data);
static int jsonld_datatypes_cb(const char *key, const char *value, void *data);

static QUILTTYPE jsonld_types[] = {
	{ "application/ld+json", "jsonld", "JSON-LD", 0.95f, 1, NULL },
	{ "application/json", "json", "JSON", 0.95f, 0, NULL },
	{ NULL, NULL, NULL, 0, 0, NULL },
};

struct jsonld_info_struct
{
	/* The request */
	QUILTREQ *req;
	/* The Content-Location */
	char *location;
	/* The primary topic */
	char *subject;
	/* The default graph */
	char *defgraph;
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
	librdf_model *model;
	librdf_iterator *iter;
	librdf_node *context;
	librdf_stream *stream;
	QUILTCANOPTS opt = QCO_CONCRETE|QCO_NOABSOLUTE;
	json_t *graph, *set;

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
	info.defgraph = quilt_canon_str(quilt_request_canonical(req), QCO_NOABSOLUTE|QCO_USERSUPPLIED);
	quilt_logf(LOG_DEBUG, "jsonld: default graph is <%s>\n", info.defgraph);
	info.subject = quilt_canon_str(quilt_request_canonical(req), QCO_SUBJECT|QCO_NOABSOLUTE);
	quilt_logf(LOG_DEBUG, "jsonld: subject is <%s>\n", info.subject);

	quilt_request_headerf(req, "Status: %d %s\n", quilt_request_status(req), quilt_request_statustitle(req));
	quilt_request_headerf(req, "Content-Type: %s\n", req->type);
	quilt_request_headerf(req, "Content-Location: %s\n", info.location);
	quilt_request_headers(req, "Vary: Accept\n");
	quilt_request_headers(req, "Server: " PACKAGE_SIGNATURE "\n");

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

	json_object_set(info.root, "@context", info.context);

	/* root/@set */
	info.rootset = json_array();

	/* root/@graph */
	info.graphs = json_array();

	model = quilt_request_model(req);
	iter = librdf_model_get_contexts(model);
	if(librdf_iterator_end(iter))
	{
		stream = librdf_model_as_stream(model);
		jsonld_serialize_stream(&info, NULL, stream, info.rootset);
		librdf_free_stream(stream);
	}
	else
	{
		for(; !librdf_iterator_end(iter); librdf_iterator_next(iter))
		{
			context = librdf_iterator_get_object(iter);
			if(!context ||
			   (info.defgraph && !strcmp(info.defgraph, jsonld_uri_node_relstr(&info, context))))
			{
				/* If context is the default graph, serialise into
				 * the root set.
				 */
				quilt_logf(LOG_DEBUG, "jsonld: found triples from default graph in model, adding to the root @set\n");
				stream = librdf_model_context_as_stream(model, context);
				jsonld_serialize_stream(&info, context, stream, info.rootset);
				librdf_free_stream(stream);
				continue;
			}

			graph = json_object();
			json_object_set_new(graph, "@id", jsonld_uri_node(&info, context));
			
			set = json_array();
			
			stream = librdf_model_context_as_stream(model, context);
			jsonld_serialize_stream(&info, context, stream, set);
			librdf_free_stream(stream);
			
			if(json_array_size(set))
			{
				json_object_set(graph, "@graph", set);
				json_array_append(info.graphs, graph);
			}

			json_decref(set);
			json_decref(graph);
		}
	}
	librdf_free_iterator(iter);

	if(json_array_size(info.rootset))
	{
		json_object_set(info.root, "@set", info.rootset);
	}
	if(json_array_size(info.graphs))
	{
		json_object_set(info.root, "@graph", info.graphs);
	}

	buf = json_dumps(info.root, JSON_PRESERVE_ORDER|JSON_ENCODE_ANY|JSON_INDENT(2));

	json_decref(info.graphs);
	json_decref(info.rootset);
	json_decref(info.context);
	json_decref(info.root);
	free(info.location);
	free(info.subject);
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
jsonld_serialize_stream(jsonld_info *info, librdf_node *context, librdf_stream *stream, json_t *targetarray)
{
	librdf_node *prevsubj, *subject, *predicate, *object;
	librdf_uri *preduri;
	const char *subjuristr;
	char *preduristr;
	librdf_statement *statement;
	json_t *kv, *entry, *prop, *array;
	size_t index;

	(void) context;

	kv = json_object();
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
			if((targetarray == info->rootset) && !strcmp(subjuristr, info->subject))
			{
				/* We're serialising the primary topic of the default graph,
				 * so we should just place this at the root for ease of
				 * parsing by non-RDF processors.
				 */
				entry = info->root;
				json_incref(entry);
				if(!json_object_get(entry, "@id"))
				{
					json_object_set_new(entry, "@id", jsonld_uri_node(info, subject));
					json_object_set_new(entry, "@type", json_null());
				}
			}
			else if((entry = json_object_get(kv, subjuristr)))
			{
				/* json_object_get() borrows the retained reference,
				 * so we incref here so that our subsequent decref doesn't
				 * do the wrong thing
				 */
				json_incref(entry);
			}
			else
			{
				/* We haven't encountered this subject within the graph yet */
				entry = json_object();
				json_object_set(kv, subjuristr, entry);
				json_array_append(targetarray, entry);
				json_object_set_new(entry, "@id", jsonld_uri_node(info, subject));
				json_object_set_new(entry, "@type", json_null());
			}
		}
		prevsubj = subject;
		prop = json_object_get(entry, preduristr);
		if(prop && !strcmp(preduristr, "@type") && json_typeof(prop) == JSON_NULL)
		{
			/* If this is the first time we're expressing
			 * rdf:type, there's a NULL placeholder that we should
			 * replace.
			 */
			json_object_set_new(entry, preduristr, jsonld_node(info, object, preduristr));
		}		
		else if(prop)
		{
			if(json_typeof(prop) != JSON_ARRAY)
			{			   
				/* Wrap the existing value in an array */
				json_incref(prop);
				array = json_array();
				json_array_append(array, prop);
				json_object_set(entry, preduristr, array);
				json_decref(prop);
				/* Borrow the reference, as json_object_get() does */
				prop = array;
				json_decref(array);
			}
			/* Append our new value to the array */
			json_array_append_new(prop, jsonld_node(info, object, preduristr));
		}
		else
		{
			json_object_set_new(entry, preduristr, jsonld_node(info, object, preduristr));
		}
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
	json_decref(kv);
	return 0;
}

/* Return a JSON object for a node */
static json_t *
jsonld_node(jsonld_info *info, librdf_node *node, const char *preduristr)
{
	json_t *obj;
	librdf_uri *nodedt;
	const char *dturi, *nodedturistr, *value;
	json_int_t intval;
	double realval;
	char *endp, *nodetype;

	endp = NULL;
	if(preduristr)
	{
		/* Is a datatype specified in the context for this predicate? */
		dturi = jsonld_predicate_datatype(info, preduristr);
	}
	else
	{
		dturi = NULL;
	}
	if(librdf_node_is_resource(node))
	{
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

/* Look up a predicate in the @context and determine whether it has a
 * specified datatype. If so, jsonld_node() can use this to encode the
 * abbreviated form of the node if the node's type matches.
 */
static const char *
jsonld_predicate_datatype(jsonld_info *info, const char *predicate)
{
	char *name;
	json_t *obj, *str;

	if(!strcmp(predicate, "@id") || !strcmp(predicate, "@type"))
	{
		/* @id and @type always have the type '@id' (i.e., resource
		 * nodes are encoded as a bare URI-string
		 */
		return "@id";
	}
	name = jsonld_predicate_locate(info, predicate, &obj);
	if(name)
	{
		free(name);
		if((str = json_object_get(obj, "@type")))
		{
			return json_string_value(str);
		}
	}
	return NULL;
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
 * either of the URI (@id) or datatype (@type) or both.
 *
 * This function always adds/updates the entry, never replaces, so it is safe to
 * do the following:
 *
 * jsonld_context_set(info, "topic", "foaf:topic", NULL);
 * jsonld_context_set(info, "topic", NULL, "@id");
 *
 * The resulting entry would be:
 *
 *    "topic": { "@id": "foaf:topic", "@type": "@id" }
 */

static int
jsonld_context_set(jsonld_info *info, const char *name, const char *uri, const char *datatype)
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
	if(add && json_object_size(obj))
	{
		json_object_set(info->context, name, obj);
	}
	json_decref(obj);
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
	jsonld_context_set(info, key, value, NULL);
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
	jsonld_context_set(info, key, NULL, value);
	return 0;
}
