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
# include <fcgiapp.h>
# include <liburi.h>
# include <librdf.h>
# include <libsparqlclient.h>
# include <syslog.h>

typedef struct quilt_request_struct QUILTREQ;
typedef struct quilt_mime_struct QUILTMIME;

struct quilt_request_struct
{
	/* The underlying FastCGI request object */
	FCGX_Request *fcgi;
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
	/* Query parameters */
	char *qbuf;
	char **query;
	int limit;
	int offset;
};

/* Logging */
void quilt_logf(int priority, const char *message, ...);
void quilt_vlogf(int priority, const char *message, va_list ap);

/* Configuration */
char *quilt_config_geta(const char *key, const char *defval);
int quilt_config_get_int(const char *key, int defval);
int quilt_config_get_bool(const char *key, int defval);
int quilt_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);

/* MIME types */
QUILTMIME *quilt_mime_create(const char *type);
QUILTMIME *quilt_mime_find(const char *type);
int quilt_mime_set_description(QUILTMIME *mime, const char *description);
int quilt_mime_set_score(QUILTMIME *mime, int score);
int quilt_mime_set_extensions(QUILTMIME *mime, const char *description);
int quilt_mime_set_callback(QUILTMIME *mime, int (*callback)(QUILTREQ *request, QUILTMIME *type));
int quilt_mime_set_hidden(QUILTMIME *mime, int hidden);
QUILTMIME *quilt_mime_negotiate(const char *accept);

/* Request processing */
QUILTREQ *quilt_request_create_fcgi(FCGX_Request *req);
int quilt_request_free(QUILTREQ *req);
int quilt_request_process(QUILTREQ *request);
int quilt_request_serialize(QUILTREQ *request);
char *quilt_request_base(void);

/* Error generation */
int quilt_error(FCGX_Request *request, int code);

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
librdf_statement *quilt_st_create(const char *subject, const char *predicate);
librdf_statement *quilt_st_create_literal(const char *subject, const char *predicate, const char *value, const char *lang);
librdf_statement *quilt_st_create_uri(const char *subject, const char *predicate, const char *value);
int quilt_model_find_double(librdf_model *model, const char *subject, const char *predicate, double *result);

#endif /*!LIBQUILT_H_*/
