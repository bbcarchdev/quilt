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

/* This file defines the interface to libquilt used by loadable modules */

#ifndef LIBQUILT_H_
# define LIBQUILT_H_										1

# include <stdarg.h>
# include <liburi.h>
# include <librdf.h>
# include <libsparqlclient.h>
# include <syslog.h>

typedef struct quilt_request_struct QUILTREQ;
typedef struct quilt_impl_struct QUILTIMPL;
typedef struct quilt_type_struct QUILTTYPE;
typedef struct quilt_canonical_struct QUILTCANON;
typedef struct quilt_bulk_struct QUILTBULK;

# ifndef QUILTIMPL_DATA_DEFINED
typedef struct quilt_impldata_struct QUILTIMPLDATA;
# endif

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
	/* The default limit */
	int deflimit;
	/* The canonical extension for the MIME type */
	const char *canonext;
	/* A helper object used to generate canonical URIs */
	QUILTCANON *canonical;
	/* The query parameters */
	char *query;
};

/* A typemap structure, filled in by a serialising plug-in for registration */
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
	/* Used internally by libquilt */
	void *data;
};

typedef enum
{
	/* The default form matches that of a request-URI */
	QCO_DEFAULT = 0,
	/* The generated URI will not be absolute */
	QCO_NOABSOLUTE = (1<<0),
	/* The generated URI will not include the path */
	QCO_NOPATH = (1<<1),
	/* The generated URI will include the resource name */
	QCO_NAME = (1<<2),
	/* The generated URI will not include an extension, even if specified in
	 * the request.
	 */
	QCO_NOEXT = (1<<3),
	/* The generated URI will always include an extension (takes precedence
	 * over QCO_NOEXT.
	 */
	QCO_FORCEEXT = (1<<4),
	/* The generated URI will not include any query parameters */
	QCO_NOPARAMS = (1<<5),
	/* The generated URI will include the fragment */
	QCO_FRAGMENT = (1<<6),

	/* Use the user-agent-supplied path and query-string if available */
	QCO_USERSUPPLIED = (1<<7),

	/* A subject URI */
	QCO_SUBJECT = (QCO_NOEXT|QCO_NOPARAMS|QCO_FRAGMENT),
	/* An abstract document URI */
	QCO_ABSTRACT = (QCO_NOEXT),
	/* A concrete document URI (i.e., Content-Location) */
	QCO_CONCRETE = (QCO_FORCEEXT|QCO_NAME),
	/* The request-URI, or something approximating it */
	QCO_REQUEST = (QCO_USERSUPPLIED)
} QUILTCANOPTS;

/* Plug-in initialisation function */
typedef int (*quilt_plugin_init_fn)(void);
/* Serializer call-back function */
typedef int (*quilt_serialize_fn)(QUILTREQ *request);
/* Engine call-back function */
typedef int (*quilt_engine_fn)(QUILTREQ *request);
/* Bulk generation call-back function */
typedef int (*quilt_bulk_fn)(QUILTBULK *context, size_t offset, size_t limit);

/* Plug-in entry-point (implemented by each plug-in) */
int quilt_plugin_init(void);

/* Plug-in handling */
int quilt_plugin_register_serializer(const QUILTTYPE *type, quilt_serialize_fn fn);
int quilt_plugin_register_engine(const char *name, quilt_engine_fn fn);
int quilt_plugin_register_bulk(const char *name, quilt_bulk_fn fn);
QUILTTYPE *quilt_plugin_serializer_first(QUILTTYPE *buf);
QUILTTYPE *quilt_plugin_next(QUILTTYPE *current);
QUILTTYPE *quilt_plugin_serializer_match_ext(const char *ext, QUILTTYPE *dest);
QUILTTYPE *quilt_plugin_serializer_match_mime(const char *mime, QUILTTYPE *dest);

/* Logging */
void quilt_logf(int priority, const char *message, ...);
void quilt_vlogf(int priority, const char *message, va_list ap);

/* Configuration */
char *quilt_config_geta(const char *key, const char *defval);
int quilt_config_get_int(const char *key, int defval);
int quilt_config_get_bool(const char *key, int defval);
int quilt_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);

/* Request processing */
const char *quilt_request_getenv(QUILTREQ *req, const char *name);
const char *quilt_request_getparam(QUILTREQ *req, const char *name);
const char *quilt_request_getparam_multi(QUILTREQ *req, const char *name);
int quilt_request_puts(QUILTREQ *req, const char *str);
int quilt_request_put(QUILTREQ *req, const unsigned char *bytes, size_t len);
int quilt_request_printf(QUILTREQ *req, const char *format, ...);
int quilt_request_vprintf(QUILTREQ *req, const char *format, va_list ap);
int quilt_request_headers(QUILTREQ *req, const char *str);
int quilt_request_headerf(QUILTREQ *req, const char *format, ...);
char *quilt_request_base(void);
int quilt_request_bulk_item(QUILTBULK *context, const char *uri);

/* Request property accessors */
QUILTIMPLDATA *quilt_request_impldata(QUILTREQ *req);
int quilt_request_serialized(QUILTREQ *req);
URI *quilt_request_uri(QUILTREQ *req);
URI *quilt_request_baseuri(QUILTREQ *req);
const char *quilt_request_baseuristr(QUILTREQ *req);
const char *quilt_request_host(QUILTREQ *req);
const char *quilt_request_ident(QUILTREQ *req);
const char *quilt_request_user(QUILTREQ *req);
const char *quilt_request_method(QUILTREQ *req);
const char *quilt_request_referer(QUILTREQ *req);
const char *quilt_request_ua(QUILTREQ *req);
const char *quilt_request_path(QUILTREQ *req);
const char *quilt_request_ext(QUILTREQ *req);
time_t quilt_request_received(QUILTREQ *req);
int quilt_request_status(QUILTREQ *req);
const char *quilt_request_statustitle(QUILTREQ *req);
const char *quilt_request_statusdesc(QUILTREQ *req);
const char *quilt_request_subject(QUILTREQ *req);
int quilt_request_home(QUILTREQ *req);
int quilt_request_index(QUILTREQ *req);
const char *quilt_request_indextitle(QUILTREQ *req);
int quilt_request_limit(QUILTREQ *req);
int quilt_request_deflimit(QUILTREQ *req);
int quilt_request_offset(QUILTREQ *req);
const char *quilt_request_type(QUILTREQ *req);
const char *quilt_request_typeext(QUILTREQ *req);
QUILTCANON *quilt_request_canonical(QUILTREQ *req);
char *quilt_request_query(QUILTREQ *req);

librdf_node *quilt_request_basegraph(QUILTREQ *req);
librdf_storage *quilt_request_storage(QUILTREQ *req);
librdf_model *quilt_request_model(QUILTREQ *req);

/* Canonical URI handling */
QUILTCANON *quilt_canon_create(QUILTCANON *source);
int quilt_canon_destroy(QUILTCANON *canon);
int quilt_canon_set_base(QUILTCANON *canon, const char *base);
int quilt_canon_set_fragment(QUILTCANON *canon, const char *path);
int quilt_canon_set_ext(QUILTCANON *canon, const char *ext);
int quilt_canon_set_explicitext(QUILTCANON *canon, const char *ext);
int quilt_canon_set_name(QUILTCANON *canon, const char *name);
int quilt_canon_reset_path(QUILTCANON *canon);
int quilt_canon_add_path(QUILTCANON *canon, const char *path);
int quilt_canon_reset_params(QUILTCANON *canon);
int quilt_canon_set_param(QUILTCANON *canon, const char *name, const char *value);
int quilt_canon_set_param_int(QUILTCANON *canon, const char *name, long value);
int quilt_canon_add_param(QUILTCANON *canon, const char *name, const char *value);
int quilt_canon_add_param_int(QUILTCANON *canon, const char *name, long value);
int quilt_canon_set_user_path(QUILTCANON *canon, const char *userpath);
int quilt_canon_set_user_query(QUILTCANON *canon, const char *userquery);
char *quilt_canon_str(QUILTCANON *canon, QUILTCANOPTS opts);

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
