#ifndef P_QUILT_H_
# define P_QUILT_H_                     1

# include <stdio.h>
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <unistd.h>
# include <time.h>
# include <inttypes.h>
# include <sys/types.h>
# include <sys/stat.h>

# include <fcgiapp.h>
# include <liburi.h>
# include <librdf.h>
# include <libxml/parser.h>
# include <curl/curl.h>
# include "libsupport.h"
# include "libnegotiate.h"
# include "libsparqlclient.h"

# define QUILT_MIME_LEN                 63

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
	/* The timestamp of request receipt */
	time_t received;
	/* The HTTP response status */
	int status;
	/* The negotiated media type */
	const char *type;
	/* The RDF model */
	librdf_storage *storage;
	librdf_model *model;
};

struct quilt_mime_struct
{
	/* The actual MIME type */
	char mimetype[QUILT_MIME_LEN+1];
	/* An array of file extensions recognised by this type. Each extension
	 * is space-separated and without a leading period. The first listed
	 * extension is considered preferred for the type.
	 */
	char *extensions;
	/* A short human-readable description of the type */
	char *desc;
	/* The server-side score for this type, from 0 to 1000. 0=never serve,
	 * 1000=always serve if supported.
	 */
	int score;
	/* If this type is supported, but not directly exposed to consumers,
	 * this flag is set
	 */
	int hidden;
	/* The callback function for serialising models as this type */
	int (*callback)(QUILTREQ *request, QUILTMIME *type);
};

/* Content negotiation */
extern NEGOTIATE *quilt_types;
extern NEGOTIATE *quilt_charsets;

/* Configuration */
int quilt_config_defaults(void);

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
int quilt_request_init(void);
QUILTREQ *quilt_request_create_fcgi(FCGX_Request *req);
int quilt_request_free(QUILTREQ *req);
int quilt_request_process(QUILTREQ *request);
int quilt_request_serialize(QUILTREQ *request);

/* Error generation */
int quilt_error(FCGX_Request *request, int code);

/* SPARQL queries */
int quilt_sparql_init(void);
int quilt_sparql_query_rdf(const char *query, librdf_model *model);

/* URL-encoding */
size_t quilt_urlencode_size(const char *src);
size_t quilt_urlencode_lsize(const char *src, size_t srclen);
int quilt_urlencode(const char *src, char *dest, size_t destlen);

/* librdf interface */
int quilt_librdf_init(void);
librdf_world *quilt_librdf_world(void);
int quilt_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen);
char *quilt_model_serialize(librdf_model *model, const char *mime);
int quilt_model_isempty(librdf_model *model);

/* FastCGI interface */
int fcgi_init(void);
int fcgi_runloop(void);

/* Processing engines */
int quilt_engine_resourcegraph_process(QUILTREQ *request);

#endif /*!P_QUILT_H_ */
