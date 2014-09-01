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

#ifndef P_LIBLIQUIFY_H_
# define P_LIBLIQUIFY_H_               1

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <errno.h>

# include "libliquify.h"

# define TABSIZE                       8

/* Part types */
# define LPT_TEXT                      0
# define LPT_VAR                       1
# define LPT_TAG  2

/* Tokens */
# define TOK_NONE                      0
# define TOK_IDENT                     'i'
# define TOK_STRING                    's'
# define TOK_VBAR                      '|'
# define TOK_END                       'e'
# define TOK_COLON                     ':'
# define TOK_COMMA                     ','
# define TOK_DOT                       '.'

/* Tokeniser flags */
# define TKF_NONE                      0
# define TKF_VAR                       (1<<0)
# define TKF_TAG                       (1<<1)
# define TKF_FILTERS                   (1<<2)
# define TKF_COLON                     (1<<3)
# define TKF_COMMA                     (1<<4)

/* Tag kinds */
# define TPK_TAG                       1
# define TPK_BEGIN                     2
# define TPK_END                       3

# define EXPR_IS(e, t) \
	((e)->root.type == TOK_NONE && \
	 (e)->root.right && \
	 (e)->root.right->type == t)

# define EXPR_IDENT(e) \
	(EXPR_IS(e, TOK_IDENT) ? (e)->root.right->text : NULL)

struct liquify_token
{
	struct liquify_token *left, *right;
	int type;
	char *text;
	size_t len;
	int line;
	int col;
};

struct liquify_expression
{
	struct liquify_token root;
	struct liquify_token *cur;
	struct liquify_token *last;
};

struct liquify_text_part
{
	char *text;
	size_t len;
};

struct liquify_filter
{
	struct liquify_filter *next;
	struct liquify_expression expr;
	struct liquify_param *pfirst, *plast;
};

struct liquify_param
{
	struct liquify_param *next;
	struct liquify_expression expr;
};

struct liquify_var_part
{
	struct liquify_expression expr;
	struct liquify_filter *ffirst, *flast;
};

struct liquify_tag_part
{
	struct liquify_expression expr;
	struct liquify_param *pfirst, *plast;
	int kind;
};

struct liquify_part
{
	struct liquify_part *next;
	int type;
	int line;
	int col;
	union
	{
		struct liquify_text_part text;
		struct liquify_var_part var;
		struct liquify_tag_part tag;
	} d;
};

struct liquify_template
{
	/* Template components */
	struct liquify_part *first, *last;
	/* Parser state */
	char *name;
	const char *start;
	size_t len;
	size_t pos;
	int line;
	int col;
};

struct liquify_capture
{
	struct liquify_capture *prev;
	struct liquify_stack *owner;
	int inhibit;
	char *buf;
	size_t buflen;
	size_t bufsize;
};

struct liquify_stack
{
	struct liquify_stack *prev;
	struct liquify_part *begin, *end;
	const char *ident;
	void *data;
};

struct liquify_context
{
	struct liquify_template *tpl;
	struct liquify_capture *capture;
	struct liquify_part *cp;
	jd_var *dict;
	char *buf;
	size_t buflen;
	size_t bufsize;
	int jumped;
	struct liquify_stack *stack;
};

/* Parse a single token */
const char *liquify_token_(struct liquify_template *tpl, struct liquify_part *part, struct liquify_expression *expr, const char *cur, int flags);
/* Free a token */
int liquify_token_free_(struct liquify_token *tok);
/* Generate a parse error */
void liquify_parse_error_(struct liquify_template *tpl, struct liquify_part *part, const char *message);
/* Parse an expression */
const char *liquify_expression_(struct liquify_template *tpl, struct liquify_part *part, struct liquify_expression *expr, const char *cur, int flags);
/* Evaluate an expression */
int liquify_eval_(struct liquify_expression *expr, jd_var *dict, jd_var *dest, int vivify);
int liquify_assign_(struct liquify_expression *expr, jd_var *dict, jd_var *newval);

/* Determine whether a tag is a block */
int liquify_is_block_(const char *name);
int liquify_block_begin_(LIQUIFYCTX *ctx, struct liquify_part *part, const char *name, struct liquify_stack *sp);
int liquify_block_end_(LIQUIFYCTX *ctx, struct liquify_part *part, const char *name, struct liquify_stack *sp);
int liquify_block_cleanup_(LIQUIFYCTX *ctx, const char *name, struct liquify_stack *stack);

/* Determine whether a tag is a non-block tag */
int liquify_is_tag_(const char *name);

/* Push a new node on the block stack */
struct liquify_stack *liquify_push_(LIQUIFYCTX *ctx, struct liquify_part *begin);
/* Pop a node off the block stack */
int liquify_pop_(LIQUIFYCTX *ctx);

/* Inhibit further output for this block */
int liquify_inhibit_(LIQUIFYCTX *ctx);

/* Jump to a particular location in the template */
int liquify_goto_(LIQUIFYCTX *ctx, struct liquify_part *where);

#endif /*!P_LIBLIQUIFY_H_*/
