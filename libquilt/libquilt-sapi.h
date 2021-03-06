/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

/* This header defines the interface to libquilt used by quiltd itself */

#ifndef LIBQUILT_INTERNAL_H_
# define LIBQUILT_INTERNAL_H_					 1

# include "libquilt.h"

typedef void (*quilt_log_fn)(int prio, const char *fmt, va_list ap);

/* This structure defines methods implemented by Quilt servers */
struct quilt_impl_struct
{
	void *reserved1;
	void *reserved2;
	void *reserved3;
	const char *(*getenv)(QUILTREQ *request, const char *name);
	const char *(*getparam)(QUILTREQ *request, const char *name);
	const char *const *(*getparam_multi)(QUILTREQ *request, const char *name);
	int (*put)(QUILTREQ *request, const unsigned char *str, size_t len);
	int (*vprintf)(QUILTREQ *request, const char *format, va_list ap);
	int (*header)(QUILTREQ *request, const unsigned char *str, size_t len);
	int (*headerf)(QUILTREQ *request, const char *format, va_list ap);
	int (*begin)(QUILTREQ *request);
	int (*end)(QUILTREQ *request);
};

struct quilt_configfn_struct
{
	size_t (*config_get)(const char *key, const char *defval, char *buf, size_t bufsize);
	char *(*config_geta)(const char *key, const char *defval);
	int (*config_get_int)(const char *key, int defval);
	int (*config_get_bool)(const char *key, int defval);
	int (*config_get_all)(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);
};

int quilt_init(quilt_log_fn logger, struct quilt_configfn_struct *fns);

/* Request processing */

QUILTREQ *quilt_request_create(QUILTIMPL *impl, QUILTIMPLDATA *data);
int quilt_request_bulk(QUILTIMPL *impl, QUILTIMPLDATA *data, size_t offset, size_t limit);
int quilt_request_free(QUILTREQ *req);
int quilt_request_process(QUILTREQ *request);
int quilt_request_serialize(QUILTREQ *request);

/* Error generation */
int quilt_error(QUILTREQ *request, int code);

#endif /*!LIBQUILT_INTERNAL_H_*/
