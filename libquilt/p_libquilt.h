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

#ifndef P_LIBQUILT_H_
# define P_LIBQUILT_H_                 1

# define _BSD_SOURCE                   1

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

# include <fcgiapp.h>
# include <liburi.h>
# include <librdf.h>
# include <libxml/parser.h>
# include <curl/curl.h>

# include "libsupport.h"
# include "libnegotiate.h"
# include "libsparqlclient.h"

# include "libquilt-internal.h"

# define QUILT_MIME_LEN                 63
# define DEFAULT_LIMIT                  25
# define MAX_LIMIT                      100

/* Not currently used */
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

/* Information about known MIME types */
struct typemap_struct
{
	const char *ext;
	const char *type;
	const char *name;
	int visible;
};

/* Content negotiation */
extern NEGOTIATE *quilt_types;
extern NEGOTIATE *quilt_charsets;

extern struct typemap_struct quilt_typemap[];

/* Logging */
int quilt_log_init_(quilt_log_fn logger);

/* Configuration */
int quilt_config_init_(struct quilt_configfn_struct *fns);

/* Request processing */
int quilt_request_init_(void);

/* librdf wrapper */
int quilt_librdf_init_(void);

/* SPARQL interface */
int quilt_sparql_init_(void);

#endif /*!P_LIBQUILT_H_ */
