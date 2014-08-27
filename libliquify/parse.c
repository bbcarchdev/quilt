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

static struct liquify_part *add_part(struct liquify_template *tpl, int type);
static struct liquify_part *add_text(struct liquify_template *tpl, int line, int col, const char *text, size_t len);
static struct liquify_filter *add_filter(struct liquify_part *part);
static struct liquify_param *add_filter_param(struct liquify_filter *filter);

static const char *parse_filter(struct liquify_template *tpl, struct liquify_part *part, struct liquify_filter *filter, const char *cur, int flags);
static const char *parse_var(struct liquify_template *tpl, const char *cur);
static const char *parse_tag(struct liquify_template *tpl, const char *start);

/* Generate a parse error */
void
liquify_parse_error_(struct liquify_template *tpl, struct liquify_part *part, const char *message)
{
	fprintf(stderr, "%s:%d:%d: %s\n", tpl->name, part->line, part->col, message);
}

int
liquify_free(struct liquify_template *template)
{
	return 0;
}

struct liquify_template *
liquify_parse(const char *name, const char *doc, size_t len)
{
	struct liquify_template *tpl;
	int startline, startcol, t;
	const char *block;

	tpl = (struct liquify_template *) calloc(1, sizeof(struct liquify_template));
	if(!tpl)
	{
		return NULL;
	}
	tpl->name = strdup(name);
	if(!tpl->name)
	{
		free(tpl);
		return NULL;
	}
	tpl->start = doc;
	tpl->len = len;
	tpl->pos = 0;
	tpl->line = 1;
	tpl->col = 1;
	while(tpl->pos < tpl->len)
	{
		/* We start each loop in 'literal text' mode, and loop until we find
		 * either the start of a tag or variable, or the end of the template,
		 * whichever comes first.
		 */
		t = LPT_TEXT;
		block = doc;
		startline = tpl->line;
		startcol = tpl->col;
		while(tpl->pos < tpl->len)
		{
			switch(*doc)
			{
				case 0:
				case '\r':
					tpl->pos++, doc++;
					break;
				case '\n':
				case '\f':
				case '\v':
					tpl->line++, tpl->pos++, doc++;
					tpl->col = 1;
					break;
				case '\t':
					tpl->pos++, doc++;
					tpl->col += TABSIZE;
					break;
				case '{':
					if(tpl->pos + 1 < tpl->len && doc[1] == '{')
					{
						/* Variables (objects) are {{ expr }} */
						t = LPT_VAR;
						break;
					}
					else if(tpl->pos + 1 < tpl->len && doc[1] == '%')
					{
						/* Tags are {% tag %} */
						t = LPT_TAG;
						break;
					}
					/* fallthrough */
				default:
					tpl->col++, tpl->pos++, doc++;
					break;
			}
			if(t)
			{
				break;
			}
		}
		if(doc > block)
		{
			/* Add the span of text from 'block' to 'doc' as
			 * a literal text part.
			 */
			if(!add_text(tpl, startline, startcol, block, doc - block))
			{
				liquify_free(tpl);
				return NULL;
			}
		}
		switch(t)
		{
		case LPT_TEXT:
			/* Do nothing */
			break;
		case LPT_VAR:
			if(!(doc = parse_var(tpl, doc)))
			{
				liquify_free(tpl);
				return NULL;
			}
			break;
		case LPT_TAG:
			if(!(doc = parse_tag(tpl, doc)))
			{
				liquify_free(tpl);
				return NULL;
			}
			break;
		}
	}
	/* Clean up the parsing context */
	tpl->start = NULL;
	tpl->len = 0;
	tpl->pos = 0;
	tpl->line = 0;
	tpl->col = 0;
	return tpl;
}


/* Add a new part to a template */
static struct liquify_part *
add_part(struct liquify_template *tpl, int type)
{
	struct liquify_part *part;
	
	part = (struct liquify_part *) calloc(1, sizeof(struct liquify_part));
	if(!part)
	{
		return NULL;
	}
	part->line = tpl->line;
	part->col = tpl->col;
	part->type = type;
	if(tpl->first)
	{
		tpl->last->next = part;
	}
	else
	{
		tpl->first = part;
	}
	tpl->last = part;
	return part;
}

/* Add a literal text part to a template */
static struct liquify_part *
add_text(struct liquify_template *tpl, int line, int col, const char *text, size_t len)
{
	struct liquify_part *part;
	
	part = add_part(tpl, LPT_TEXT);
	if(!part)
	{
		return NULL;
	}
	part->line = line;
	part->col = col;
	part->d.text.text = (char *) malloc(len + 1);
	if(!part->d.text.text)
	{
		free(part);
		return NULL;
	}
	strncpy(part->d.text.text, text, len);
	part->d.text.text[len] = 0;
	part->d.text.len = len;
	return part;
}

static struct liquify_filter *
add_filter(struct liquify_part *part)
{
	struct liquify_filter *p;
	
	if(part->type != LPT_VAR)
	{
		return NULL;
	}
	p = (struct liquify_filter *) calloc(1, sizeof(struct liquify_filter));
	if(!p)
	{
		return NULL;
	}
	if(part->d.var.ffirst)
	{
		part->d.var.flast->next = p;
	}
	else
	{
		part->d.var.ffirst = p;
	}
	part->d.var.flast = p;
	return p;
}

static struct liquify_param *
add_filter_param(struct liquify_filter *filter)
{
	struct liquify_param *p;
	
	p = (struct liquify_param *) calloc(1, sizeof(struct liquify_param));
	if(!p)
	{
		return NULL;
	}
	if(filter->pfirst)
	{
		filter->plast->next = p;
	}
	else
	{
		filter->pfirst = p;
	}
	filter->plast = p;
	return p;
}

/* parse a filter, which has the form 'foo' or 'foo:"param1","param2"' */
static const char *
parse_filter(struct liquify_template *tpl, struct liquify_part *part, struct liquify_filter *filter, const char *cur, int flags)
{
	struct liquify_expression *expr;
	struct liquify_param *param;
	
	expr = &(filter->expr);
	cur = liquify_expression_(tpl, part, expr, cur, flags | TKF_COLON);
	if(!cur)
	{
		return NULL;
	}
	if(!expr->last)
	{
		liquify_parse_error_(tpl, part, "expected: end-of-tag, vertical bar, or colon");
		return NULL;
	}
	if(expr->last->type == TOK_END || expr->last->type == TOK_VBAR)
	{
		return cur;
	}
	/* TOK_COLON - parameters follow */
	liquify_token_free_(expr->last);
	expr->last = NULL;
	while(tpl->pos < tpl->len)
	{
		param = add_filter_param(filter);
		if(!param)
		{
			return NULL;
		}
		expr = &(param->expr);
		cur = liquify_expression_(tpl, part, expr, cur, flags | TKF_COMMA);
		if(!cur)
		{
			return NULL;
		}
		if(!expr->last)
		{
			liquify_parse_error_(tpl, part, "expected: end-of-tag, vertical bar, or comma");
			return NULL;
		}
		if(expr->last->type == TOK_END || expr->last->type == TOK_VBAR)
		{
			filter->expr.last = expr->last;
			return cur;
		}
		/* TOK_COMMA - loop */
	}
	liquify_parse_error_(tpl, part, "unexpected end of template");
	return NULL;
}

/* Parse a 'variable' (output markup):
 *   {{ expr }}
 *   {{ expr | filter }}
 *   {{ expr | filter:"param","param","..." | filter2 }}
 */
static const char *
parse_var(struct liquify_template *tpl, const char *cur)
{
	struct liquify_part *part;
	struct liquify_expression *expr;
	struct liquify_filter *filter;
	int t;
	
	part = add_part(tpl, LPT_VAR);
	if(!part)
	{
		return NULL;
	}
	cur += 2;
	tpl->pos += 2;
	tpl->col += 2;
	expr = &part->d.var.expr;
	cur = liquify_expression_(tpl, part, expr, cur, TKF_VAR|TKF_FILTERS);
	if(!cur)
	{
		return NULL;
	}
	if(expr->last)
	{
		t = expr->last->type;
	}
	else
	{
		t = TOK_NONE;
	}
	liquify_token_free_(expr->last);
	expr->last = NULL;
	while(tpl->pos < tpl->len)
	{
		if(t == TOK_END)
		{
			return cur;
		}
		if(t != TOK_VBAR)
		{
			liquify_parse_error_(tpl, part, "expected: end-of-variable ('}}') or filter");
			return NULL;
		}
		filter = add_filter(part);
		if(!filter)
		{
			return NULL;
		}
		cur = parse_filter(tpl, part, filter, cur, TKF_VAR|TKF_FILTERS);
		if(!cur)
		{
			return NULL;
		}
		t = filter->expr.last->type;
	}
	liquify_parse_error_(tpl, part, "unexpected end of template");
	return NULL;
}

/* Parse a tag:
 *  {% name %}
 *  {% name parameters... %}
 */
static const char *
parse_tag(struct liquify_template *tpl, const char *cur)
{
	const char *end;
	struct liquify_part *part;
	struct liquify_expression *expr;
	int q, finished;
	size_t varlen;
	
	part = add_part(tpl, LPT_TAG);
	if(!part)
	{
		return NULL;
	}
	expr = &(part->d.tag.expr);
	cur += 2, tpl->pos += 2, tpl->col += 2;
	cur = liquify_expression_(tpl, part, expr, cur, TKF_TAG);
	if(!cur)
	{
		return NULL;
	}
	finished = 0;
	if(expr->last)
	{
		if(expr->last->type == TOK_END)
		{
			finished = 1;
		}
		else
		{
			liquify_parse_error_(tpl, part, "expected: end-of-tag ('%}') or parameters");
			return NULL;
		}
		liquify_token_free_(expr->last);
		expr->last = NULL;
	}
	q = 0;
	end = cur;
	while(!finished && tpl->pos < tpl->len)
	{
		if(q && *end == q)
		{
			q = 0;
			tpl->pos++, tpl->col++, end++;
			continue;
		}
		switch(*end)
		{
		case 0:
		case '\r':
			tpl->pos++, end++;
			break;
		case '\n':
		case '\f':
		case '\v':
			tpl->pos++, tpl->line++, end++;
			tpl->col = 1;
			break;
		case '\t':
			tpl->pos++, end++;
			tpl->col += TABSIZE;
			break;
		case '\'':
		case '"':
			if(!q)
			{
				q = *end;
			}
			tpl->pos++, tpl->col++, end++;
			break;
		case '%':
			if(q)
			{
				tpl->pos++, tpl->col++, end++;
				break;
			}
			if(tpl->pos + 1 < tpl->len && end[1] == '}')
			{
				tpl->pos += 2, tpl->col += 2, end += 2;
				finished = 1;
				break;
			}
			/* fallthrough */
		default:
			tpl->pos++, tpl->col++, end++;
		}
		if(finished)
		{
			break;
		}
	}
	if(!finished)
	{
		liquify_parse_error_(tpl, part, "expected end-of-tag ('%}'), but reached end of template");
		return NULL;
	}
	if(end > cur)
	{
		varlen = end - cur;
		part->d.tag.len = varlen;
		part->d.tag.text = (char *) malloc(varlen + 1);
		if(!part->d.tag.text)
		{
			return NULL;
		}
		strncpy(part->d.tag.text, cur, varlen);
		part->d.tag.text[varlen] = 0;
	}
	return end;
}
