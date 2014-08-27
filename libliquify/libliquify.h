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
# include <jsondata.h>

typedef struct liquify_template LIQUIFY;
typedef struct liquify_context LIQUIFYCTX;

/* Parse a template */
LIQUIFY *liquify_parse(const char *name, const char *doc, size_t len);
/* Free a template */
int liquify_free(LIQUIFY *template);
/* Dump the contents of a parsed template */
int liquify_dump(LIQUIFY *tpl, FILE *f);
/* Apply a template, returning its contents as a string */
char *liquify_apply(LIQUIFY *tpl, jd_var *dict);
/* Write text to the current processing context */
int liquify_emit(LIQUIFYCTX *ctx, const char *str, size_t len);
/* Write a JSON value to the current context */
int liquify_emit_json(LIQUIFYCTX *ctx, jd_var *value);
/* Begin capturing output to a buffer */
int liquify_capture(LIQUIFYCTX *ctx);
/* Finish capturing */
char *liquify_capture_end(LIQUIFYCTX *ctx, size_t *len);

#endif /*!LIBLIQUIFY_H_*/
