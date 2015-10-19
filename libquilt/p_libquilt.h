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

#ifndef P_LIBQUILT_H_
# define P_LIBQUILT_H_                 1

# include <stdio.h>
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <unistd.h>
# include <time.h>
# include <inttypes.h>
# include <ctype.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <dlfcn.h>
# include <errno.h>

# include <liburi.h>
# include <librdf.h>
# include <libxml/parser.h>
# include <curl/curl.h>

# include "libnegotiate.h"
# include "libsparqlclient.h"

# include "libquilt-sapi.h"

# define PLUGINDIR                      LIBDIR "/quilt/"
# define QUILT_MIME_LEN                 64
# define DEFAULT_LIMIT                  25
# define MAX_LIMIT                      100

# ifndef HAVE_STRLCPY
#  undef strlcpy
#  define strlcpy(dest, src, buflen) \
	if(buflen > 1) \
	{ \
	strncpy(dest, src, buflen - 1); \
	dest[buflen - 1] = 0; \
	} \
	else if(buflen) \
	{ \
	dest[0] = 0; \
	}
#endif /*!HAVE_STRLCPY*/

# ifndef HAVE_STRLCAT
#  undef strlcat
#  define strlcat(dest, src, buflen) \
	if(buflen > 1) \
	{ \
	strncat(dest, src, buflen - 1); \
	dest[buflen - 1] = 0; \
	} \
	else if(buflen) \
	{ \
	dest[0] = 0; \
	}
#endif /*!HAVE_STRLCAT*/

typedef struct quilt_mime_struct QUILTMIME;
typedef struct quilt_callback_struct QUILTCB;

typedef enum
{
	/* 0 */ QCB_NONE,
	/* 1 */ QCB_SERIALIZE,
	/* 2 */ QCB_ENGINE,
	/* 3 */ QCB_BULK
} QUILTCBTYPE;

/* This structure is dynamically-allocated, and represents a MIME type that
 * has been registered by a plugin; the constant version, passed to the
 * plugin registration functions, is QUILTTYPE (defined in libquilt.h)
 */
struct quilt_mime_struct
{
	/* The actual MIME type */
	char mimetype[QUILT_MIME_LEN];
	/* A list of file extensions recognised by this type. Each extension
	 * is space-separated and without a leading period. The first listed
	 * extension is considered preferred for the type.
	 */
	char **extensions;
	/* A short human-readable description of the type */
	char *desc;
	/* The server-side score for this type, from 0 to 1000. 0=never serve,
	 * 1000=always serve if supported.
	 */
	float qs;
	/* If this type is supported, but not directly exposed to consumers,
	 * this flag is unset.
	 */
	int visible;
};

/* The list of registered callbacks */
struct quilt_callback_struct
{
	QUILTCB *prev, *next;
	void *handle;
	char *name;
	QUILTMIME *mime;
	QUILTCBTYPE type;
	union {
		quilt_serialize_fn serialize;
		quilt_engine_fn engine;
		quilt_bulk_fn bulk;
	} cb;
};

struct quilt_canon_param_struct
{
	char *name;
	char *value;
};

struct quilt_canonical_struct
{
	/* The absolute base - scheme, auth, port, host */
	char *base;
	/* The site-relative path (i.e., begins with '/') */
	char *path;
	/* The local resource name if not included in the path */
	char *name;
	/* The default file extension for this type */
	char *ext;
	/* The explicitly-specified file extension for this type */
	char *explicitext;
	/* The fragment for this resource */
	char *fragment;
	/* Sorted list of parameters */
	struct quilt_canon_param_struct *params;
	size_t nparams;
};

/* Bulk-generation context */
struct quilt_bulk_struct
{
	QUILTIMPL *impl;
	QUILTIMPLDATA *data;
	QUILTCB *callback;
	size_t limit;
	size_t offset;
};

/* Content negotiation */
extern NEGOTIATE *quilt_types_;
extern NEGOTIATE *quilt_charsets_;

/* Logging */
int quilt_log_init_(quilt_log_fn logger);

/* Configuration */
int quilt_config_init_(struct quilt_configfn_struct *fns);

/* Request processing */
int quilt_request_init_(void);
int quilt_request_sanity_(void);
QUILTREQ *quilt_request_create_uri_(QUILTIMPL *impl, QUILTIMPLDATA *data, const char *uri);

/* librdf wrapper */
int quilt_librdf_init_(void);

/* SPARQL interface */
int quilt_sparql_init_(void);

/* Plug-ins */
int quilt_plugin_init_(void);
int quilt_plugin_load_(const char *name);
int quilt_plugin_unload_(void *handle);
QUILTCB *quilt_plugin_cb_add_(void *handle, const char *name);
QUILTCB *quilt_plugin_cb_find_mime_(QUILTCBTYPE type, const char *mimetype);
QUILTCB *quilt_plugin_cb_find_name_(QUILTCBTYPE type, const char *name);
int quilt_plugin_invoke_engine_(QUILTCB *cb, QUILTREQ *req);
int quilt_plugin_invoke_serialize_(QUILTCB *cb, QUILTREQ *req);
int quilt_plugin_invoke_bulk_(QUILTCB *cb, QUILTBULK *bulk);

#endif /*!P_LIBQUILT_H_ */
