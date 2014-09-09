/* libliquify is a simple templating engine which uses a subset of the
 * Liquid template syntax <https://github.com/Shopify/liquid/wiki>
 */

/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC.
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

#ifndef LIBLIQUIFY_H_
# define LIBLIQUIFY_H_                 1

# include <stdio.h>
# include <stddef.h>
# include <stdarg.h>
# include <jsondata.h>

typedef struct liquify_struct LIQUIFY;
typedef struct liquify_template_struct LIQUIFYTPL;
typedef struct liquify_context_struct LIQUIFYCTX;

/* Create a liquify environment */
LIQUIFY *liquify_create(void);
/* Destroy a liquify environment */
int liquify_destroy(LIQUIFY *env);
/* Set the logging callback for a liquify environment */
int liquify_set_logger(LIQUIFY *liquify, void (*logger)(int level, const char *fmt, va_list ap));
/* Set the loader callback for a liquify environment */
int liquify_set_loader(LIQUIFY *liquify, LIQUIFYTPL *(*loader)(LIQUIFY *env, const char *name, void *dta), void *data);

/* Load a template, using the currently-defined loader */
LIQUIFYTPL *liquify_load(LIQUIFY *liquify, const char *name);

/* Log a message within a liquify environment */
void liquify_logf(LIQUIFY *liquify, int level, const char *fmt, ...);
void liquify_vlogf(LIQUIFY *liquify, int level, const char *fmt, va_list ap);

/* Allocate zero-filled memory (logs error and aborts on failure) */
void *liquify_alloc(LIQUIFY *liquify, size_t len);
void *liquify_realloc(LIQUIFY *restrict liquify, void *restrict ptr, size_t newlen);
char *liquify_strdup(LIQUIFY *restrict liquify, const char *src);
void liquify_free(LIQUIFY *restrict liquify, void *restrict ptr);

/* Parse a template */
LIQUIFYTPL *liquify_parse(LIQUIFY *env, const char *name, const char *doc, size_t len);

/* Dump the contents of a parsed template */
int liquify_dump(LIQUIFYTPL *tpl, FILE *f);

/* Locate a loaded template by name */
LIQUIFYTPL *liquify_locate(LIQUIFY *env, const char *name);
/* Apply a template, returning its contents as a string */
char *liquify_apply_name(LIQUIFY *env, const char *name, jd_var *dict);
char *liquify_apply(LIQUIFYTPL *tpl, jd_var *dict);

/* Write text to the current processing context */
int liquify_emit(LIQUIFYCTX *ctx, const char *str, size_t len);
/* Write a null-terminated string to the current processing context */
int liquify_emit_str(LIQUIFYCTX *ctx, const char *str);
/* Write a JSON value to the current context */
int liquify_emit_json(LIQUIFYCTX *ctx, jd_var *value);

/* Begin capturing output to a buffer */
int liquify_capture(LIQUIFYCTX *ctx);
/* Finish capturing */
char *liquify_capture_end(LIQUIFYCTX *ctx, size_t *len);

#endif /*!LIBLIQUIFY_H_*/
