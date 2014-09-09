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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libliquify.h"

#define BLOCKSIZE                      128
#define ROUNDUP(n)                     ((((n) / BLOCKSIZE) + 1) * BLOCKSIZE)

static int apply_filter(LIQUIFYCTX *ctx, char *buf, size_t len, struct liquify_filter *filter);

/* Locate a loaded template by name */
LIQUIFYTPL *
liquify_locate(LIQUIFY *env, const char *name)
{
	LIQUIFYTPL *tpl;
	
	for(tpl = env->first; tpl; tpl = tpl->next)
	{
		if(!strcmp(tpl->name, name))
		{
			return tpl;
		}
	}
	return NULL;
}

char *
liquify_apply_name(LIQUIFY *env, const char *name, jd_var *dict)
{
	LIQUIFYTPL *tpl;
	
	tpl = liquify_locate(env, name);
	if(!tpl)
	{
		liquify_logf(env, LOG_ERR, "failed to locate template '%s'\n", name);
		return NULL;
	}
	return liquify_apply(tpl, dict);
}

char *
liquify_apply(LIQUIFYTPL *template, jd_var *dict)
{
	LIQUIFYCTX context;
	struct liquify_part *part;
	struct liquify_filter *filter;
	struct liquify_stack *sp;
	jd_var value = JD_INIT;
	size_t len;
	int r;
	char *str, *buf;
	
	memset(&context, 0, sizeof(LIQUIFYCTX));
	context.tpl = template;
	context.cp = template->first;
	context.dict = dict;
	str = NULL;
	r = 0;
	while(context.cp)
	{
		part = context.cp;
		context.jumped = 0;
		switch(part->type)
		{
		case LPT_TEXT:
			if(context.capture && context.capture->inhibit)
			{
				break;
			}
			if(liquify_emit(&context, part->d.text.text, part->d.text.len))
			{
				liquify_free(template->env, context.buf);
				return NULL;
			}
			break;
		case LPT_VAR:
			if(context.capture && context.capture->inhibit)
			{
				break;
			}
			JD_SCOPE
			{
				liquify_eval_(&(part->d.var.expr), dict, &value, 0);
				if(part->d.var.ffirst)
				{
					if(liquify_capture(&context))
					{
						jd_release(&value);
						r = -1;
					}
				}
				if(!r)
				{
					liquify_emit_json(&context, &value);
					jd_release(&value);
					if(part->d.var.ffirst)
					{
						for(filter = part->d.var.ffirst; filter; filter = filter->next)
						{
							buf = context.capture->buf;
							len = context.capture->buflen;
							context.capture->buf = NULL;
							context.capture->buflen = 0;
							context.capture->bufsize = 0;
							if(apply_filter(&context, buf, len, filter))
							{
								r = -1;
								break;
							}
							liquify_free(template->env, buf);
						}
						len = 0;
						str = liquify_capture_end(&context, &len);
						if(!r)
						{
							liquify_emit(&context, str, len);
						}
					}
				}
			}
			break;
		case LPT_TAG:
			if(part->d.tag.kind == TPK_END)
			{
				if(!context.stack ||
				   (context.stack->end && context.stack->end != part) ||
				   strcmp(context.stack->ident, EXPR_IDENT(&(part->d.tag.expr)) + 3))
				{
					liquify_logf(template->env, LOG_ERR, "%s:%d:%d: tag mismatch: %s does not match %s\n", template->name, part->line, part->col, EXPR_IDENT(&(part->d.tag.expr)), context.stack->ident);
					r = -1;
					break;
				}
				context.stack->end = part;
				if(context.capture && context.capture->inhibit)
				{
					sp = context.stack;
					liquify_block_cleanup_(&context, context.stack->ident, context.stack);
					if(context.capture->owner == sp)
					{
						liquify_capture_end(&context, NULL);
					}
					liquify_pop_(&context);
					break;
				}
				
				if(liquify_block_end_(&context, part, context.stack->ident, context.stack))
				{
					r = -1;
					break;
				}
				if(!context.jumped)
				{
					liquify_block_cleanup_(&context, context.stack->ident, context.stack);
					liquify_pop_(&context);
				}
			}
			else if(part->d.tag.kind == TPK_BEGIN)
			{
				if(context.capture && context.capture->inhibit)
				{
					if(!liquify_push_(&context, part))
					{
						r = 1;
					}
					break;
				}
				if(!context.stack || context.stack->begin != part)
				{
					if(!liquify_push_(&context, part))
					{
						r = -1;
						break;
					}
				}
				if(liquify_block_begin_(&context, part, EXPR_IDENT(&(part->d.tag.expr)), context.stack))
				{
					r = -1;
					break;
				}
			}
			else if(part->d.tag.kind == TPK_TAG)
			{
				if(liquify_tag_(&context, part, EXPR_IDENT(&(part->d.tag.expr))))
				{
					r = -1;
					break;
				}
			}
			break;
		}
		if(!context.jumped)
		{
			context.cp = context.cp->next;
		}
	}
	if(r)
	{
		liquify_free(template->env, context.buf);
		liquify_free(template->env, str);
		return NULL;
	}
	if(!context.buf)
	{
		return liquify_strdup(template->env, "");
	}
	return context.buf;
}

/* Write a JSON value to the current context */
int
liquify_emit_json(LIQUIFYCTX *ctx, jd_var *value)
{
	jd_var str = JD_INIT;
	size_t len;
	const char *p;
	int r;
	
	JD_SCOPE
	{		
		jd_stringify(&str, value);
		len = 0;
		p = jd_bytes(&str, &len);
		if(len)
		{
			r = liquify_emit(ctx, p, len - 1);
		}
	}
	return r;
}

/* Write text to the current context */
int
liquify_emit(LIQUIFYCTX *ctx, const char *str, size_t slen)
{
	size_t newsize, *size, *len;
	char **buf;
	
	if(ctx->capture)
	{
		buf = &(ctx->capture->buf);
		size = &(ctx->capture->bufsize);
		len = &(ctx->capture->buflen);
	}
	else
	{
		buf = &(ctx->buf);
		size = &(ctx->bufsize);
		len = &(ctx->buflen);
	}
	if(*len + slen + 1 > *size)
	{
		newsize = *size + ROUNDUP(slen);
		*buf = (char *) liquify_realloc(ctx->tpl->env, *buf, newsize);
		*size = newsize;
	}
	memcpy(&((*buf)[*len]), str, slen);
	*len += slen;
	(*buf)[*len] = 0;
	return 0;
}

/* Write a null-terminated string to the current processing context */
int
liquify_emit_str(LIQUIFYCTX *ctx, const char *str)
{
	return liquify_emit(ctx, str, strlen(str));
}

/* Begin capturing output to a buffer */
int
liquify_capture(LIQUIFYCTX *ctx)
{
	struct liquify_capture *p;
	
	p = (struct liquify_capture *) liquify_alloc(ctx->tpl->env, sizeof(struct liquify_capture));
	if(!p)
	{
		return -1;
	}
	p->prev = ctx->capture;
	ctx->capture = p;
	return 0;
}

/* Inhibit further output for this block */
int
liquify_inhibit_(LIQUIFYCTX *ctx)
{
	if(liquify_capture(ctx))
	{
		return -1;
	}
	ctx->capture->inhibit = 1;
	ctx->capture->owner = ctx->stack;
	return 0;
}

/* Finish capturing */
char *
liquify_capture_end(LIQUIFYCTX *ctx, size_t *len)
{
	struct liquify_capture *p;
	char *buf;
	
	if(!ctx->capture)
	{
		return NULL;
	}
	p = ctx->capture;
	ctx->capture = p->prev;
	buf = p->buf;
	if(len)
	{
		*len = p->buflen;
	}
	if(p->inhibit)
	{
		liquify_free(ctx->tpl->env, p);
		return NULL;
	}
	liquify_free(ctx->tpl->env, p);
	if(!buf)
	{
		return liquify_strdup(ctx->tpl->env, "");
	}
	return buf;
}

static int
apply_filter(LIQUIFYCTX *ctx, char *buf, size_t len, struct liquify_filter *filter)
{	
	return liquify_filter_apply_(ctx, filter->expr.root.right->text, buf, len);
}

int
liquify_goto_(LIQUIFYCTX *ctx, struct liquify_part *where)
{
	ctx->cp = where;
	ctx->jumped = 1;
	return 0;
}

struct liquify_stack *
liquify_push_(LIQUIFYCTX *ctx, struct liquify_part *begin)
{
	struct liquify_stack *node;

	node = (struct liquify_stack *) liquify_alloc(ctx->tpl->env, sizeof(struct liquify_stack));
	node->begin = begin;
	node->prev = ctx->stack;
	node->ident = EXPR_IDENT(&(begin->d.tag.expr));
	ctx->stack = node;
	return node;
}

int
liquify_pop_(LIQUIFYCTX *ctx)
{
	struct liquify_stack *node;

	if(!ctx->stack)
	{
		errno = EINVAL;
		return -1;
	}
	node = ctx->stack;
	ctx->stack = node->prev;
	liquify_free(ctx->tpl->env, node);
	return 0;
}
