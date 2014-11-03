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

/* This file defines the interface to libquilt used by loadable modules */

#ifndef LIBQUILT_H_
# define LIBQUILT_H_                    1

# include <stdarg.h>
# include <liburi.h>
# include <librdf.h>
# include <libsparqlclient.h>
# include <syslog.h>

typedef struct quilt_request_struct QUILTREQ;
typedef struct quilt_impl_struct QUILTIMPL;
typedef struct quilt_type_struct QUILTTYPE;

# ifndef QUILTIMPL_DATA_DEFINED
typedef struct quilt_impldata_struct QUILTIMPLDATA;
# endif

struct quilt_impl_struct
{
	void *reserved1;
	void *reserved2;
	void *reserved3;
	const char *(*getenv)(QUILTREQ *request, const char *name);
	const char *(*getparam)(QUILTREQ *request, const char *name);
	int (*put)(QUILTREQ *request, const char *str, size_t len);
	int (*printf)(QUILTREQ *request, const char *format, ...);
	int (*vprintf)(QUILTREQ *request, const char *format, va_list ap);
};

struct quilt_request_struct
{
	/* Pointer to the implementation */
	QUILTIMPL *impl;
	/* Implementation-specific data */
	QUILTIMPLDATA *data;
	int serialized;
	/* Request parameters */
	URI *uri;
	const char *host;
	const char *ident;
	const char *user;
	const char *method;
	const char *referer;
	const char *ua;
	char *path;
	char *ext;
	/* The base URI */
	URI *baseuri;
	char *base;
	librdf_node *basegraph;
	/* The timestamp of request receipt */
	time_t received;
	/* The HTTP response status */
	int status;
	const char *statustitle;
	/* The negotiated media type */
	const char *type;
	/* The RDF model */
	librdf_storage *storage;
	librdf_model *model;
	/* The URI of the query-subject, without any fragment */
	char *subject;
	/* Is the root resource? */
	int home;
	/* Is this an index resource? */
	int index;
	const char *indextitle;
	/* If an error, a description of the error condition */
	const char *errordesc;
	/* Query parameters */
	int limit;
	int offset;
};

struct quilt_type_struct
{
	/* The actual MIME type */
	const char *mimetype;
	/* A list of file extensions recognised by this type. Each extension
	 * is space-separated and without a leading period. The first listed
	 * extension is considered preferred for the type.
	 */
	const char *extensions;
	/* A short human-readable description of the type */
	const char *desc;
	/* The server-side score for this type, from 0 to 1.0. 0=never serve,
	 * 1.0=always serve if supported.
	 */
	float qs;
	/* If this type is supported, but not directly exposed to consumers,
	 * this flag is unset.
	 */
	int visible;
};

/* Plug-in initialisation function */
typedef int (*quilt_plugin_init_fn)(void);
/* Serializer call-back function */
typedef int (*quilt_serialize_fn)(QUILTREQ *request);
/* Engine call-back function */
typedef int (*quilt_engine_fn)(QUILTREQ *request);

/* Plug-in entry-point (implemented by each plug-in) */
int quilt_plugin_init(void);

/* Plug-in handling */
int quilt_plugin_register_serializer(const QUILTTYPE *type, quilt_serialize_fn fn);
int quilt_plugin_register_engine(const char *name, quilt_engine_fn fn);

/* Logging */
void quilt_logf(int priority, const char *message, ...);
void quilt_vlogf(int priority, const char *message, va_list ap);

/* Configuration */
char *quilt_config_geta(const char *key, const char *defval);
int quilt_config_get_int(const char *key, int defval);
int quilt_config_get_bool(const char *key, int defval);
int quilt_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);

/* Request processing */
char *quilt_request_base(void);

/* SPARQL queries */
SPARQL *quilt_sparql(void);
int quilt_sparql_query_rdf(const char *query, librdf_model *model);

/* URL-encoding */
size_t quilt_urlencode_size(const char *src);
size_t quilt_urlencode_lsize(const char *src, size_t srclen);
int quilt_urlencode(const char *src, char *dest, size_t destlen);

/* librdf interface */
librdf_world *quilt_librdf_world(void);
int quilt_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen);
char *quilt_model_serialize(librdf_model *model, const char *mime);
int quilt_model_isempty(librdf_model *model);
char *quilt_uri_contract(const char *uri);
librdf_node *quilt_node_create_uri(const char *uri);
librdf_node *quilt_node_create_literal(const char *value, const char *lang);
librdf_node *quilt_node_create_int(int value);
librdf_statement *quilt_st_create(const char *subject, const char *predicate);
librdf_statement *quilt_st_create_literal(const char *subject, const char *predicate, const char *value, const char *lang);
librdf_statement *quilt_st_create_uri(const char *subject, const char *predicate, const char *value);
int quilt_model_find_double(librdf_model *model, const char *subject, const char *predicate, double *result);

#endif /*!LIBQUILT_H_*/
