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

char *
liquify_apply(struct liquify_template *template, jd_var *dict)
{
	LIQUIFYCTX context;
	struct liquify_part *part;
	struct liquify_filter *filter;
	jd_var empty = JD_INIT;
	jd_var *value;
	size_t len;
	int r;
	char *str, *buf;
	
	memset(&context, 0, sizeof(LIQUIFYCTX));
	context.tpl = template;
	part = template->first;
	str = NULL;
	r = 0;
	while(part)
	{
		switch(part->type)
		{
		case LPT_TEXT:
			if(liquify_emit(&context, part->d.text.text, part->d.text.len))
			{
				free(context.buf);
				return NULL;
			}
			break;
		case LPT_VAR:
			JD_SCOPE
			{
				value = &empty;
				liquify_eval_(&(part->d.var.expr), dict, &value);
				if(part->d.var.ffirst)
				{
					if(liquify_capture(&context))
					{
						if(value != &empty)
						{
							jd_release(value);
						}
						r = -1;
					}
				}
				if(!r)
				{
					liquify_emit_json(&context, value);
					if(value != &empty)
					{
						jd_release(value);
					}
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
							free(buf);
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
		}
		part = part->next;
	}
	if(r)
	{
		free(context.buf);
		free(str);
		return NULL;
	}
	if(!context.buf)
	{
		return strdup("");
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
	char *p, **buf;
	
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
		p = (char *) realloc(*buf, newsize);
		if(!p)
		{
			return -1;
		}
		*buf = p;
		*size = newsize;
	}
	memcpy(&((*buf)[*len]), str, slen);
	*len += slen;
	(*buf)[*len] = 0;
	return 0;
}

/* Begin capturing output to a buffer */
int
liquify_capture(LIQUIFYCTX *ctx)
{
	struct liquify_capture *p;
	
	p = (struct liquify_capture *) calloc(1, sizeof(struct liquify_capture));
	if(!p)
	{
		return -1;
	}
	p->prev = ctx->capture;
	ctx->capture = p;
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
	free(p);
	if(!buf)
	{
		return strdup("");
	}
	return buf;
}

static int
apply_filter(LIQUIFYCTX *ctx, char *buf, size_t len, struct liquify_filter *filter)
{	
	fprintf(stderr, "%s:%d:%d: no such filter '%s'\n", ctx->tpl->name, filter->expr.root.right->line, filter->expr.root.right->col, filter->expr.root.right->text);
	
	liquify_emit(ctx, "{ filter: ", 10);
	liquify_emit(ctx, filter->expr.root.right->text, filter->expr.root.right->len);
	liquify_emit(ctx, " [", 2);
	liquify_emit(ctx, buf, len);
	liquify_emit(ctx, "] }", 3);
	return 0;
}
