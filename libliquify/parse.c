/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC.
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

static struct liquify_part *add_part(LIQUIFYTPL *tpl, int type);
static struct liquify_part *add_text(LIQUIFYTPL *tpl, int line, int col, const char *text, size_t len);
static struct liquify_filter *add_filter(LIQUIFYTPL *tpl, struct liquify_part *part);
static struct liquify_param *add_filter_param(LIQUIFYTPL *tpl, struct liquify_filter *filter);

static const char *parse_filter(LIQUIFYTPL *tpl, struct liquify_part *part, struct liquify_filter *filter, const char *cur, int flags);
static const char *parse_var(LIQUIFYTPL *tpl, const char *cur);
static const char *parse_tag(LIQUIFYTPL *tpl, const char *start);

static int liquify_tpl_free_part_(LIQUIFY *env, struct liquify_part *part);
static int liquify_tpl_free_expr_contents_(LIQUIFY *env, struct liquify_expression *expr);
static int liquify_tpl_free_token_(LIQUIFY *env, struct liquify_token *token);

/* Parse a named template (consisting of len bytes beginning at doc), and
 * add it to the provided environment. If a template of the same name
 * exists in the environment already, it will be replaced.
 */
LIQUIFYTPL *
liquify_parse(LIQUIFY *env, const char *name, const char *doc, size_t len)
{
	LIQUIFYTPL *prev, *p, *tpl;
	int startline, startcol, t;
	const char *block;

	tpl = (LIQUIFYTPL *) liquify_alloc(env, sizeof(LIQUIFYTPL));
	tpl->env = env;
	tpl->name = liquify_strdup(env, name);
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
				liquify_tpl_free_(tpl);
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
				liquify_tpl_free_(tpl);
				return NULL;
			}
			break;
		case LPT_TAG:
			if(!(doc = parse_tag(tpl, doc)))
			{
				liquify_tpl_free_(tpl);
				return NULL;
			}
			break;
		}
	}
	if(tpl->stack)
	{
		PARTERR(tpl, tpl->stack, "{%% %s %%} has no matching {%% end%s %%} before end of template is reached\n", EXPR_IDENT(&(tpl->stack->d.tag.expr)), EXPR_IDENT(&(tpl->stack->d.tag.expr)));
		liquify_tpl_free_(tpl);
		return NULL;
	}
	/* Clean up the parsing context */
	tpl->start = NULL;
	tpl->len = 0;
	tpl->pos = 0;
	tpl->line = 0;
	tpl->col = 0;
	/* Add the template to the environment */
	prev = NULL;
	for(p = env->first; p; p = p->next)
	{
		if(!strcmp(p->name, tpl->name))
		{
			/* A template with the same name exists, replace it */
			tpl->next = p->next;
			if(prev)
			{
				prev->next = tpl;
			}
			if(env->first == p)
			{
				env->first = tpl;
			}
			if(env->last == p)
			{
				env->last = tpl;
			}
			liquify_tpl_free_(p);
			return tpl;
		}
		prev = p;
	}
	/* A template with this name doesn't already exist, add it to the
	 * linked list.
	 */
	if(env->last)
	{
		env->last->next = tpl;
	}
	else
	{
		env->first = tpl;
	}
	env->last = tpl;
	return tpl;
}

/* Free a template; note that this function will not manage the linked list
 * of templates within an environment, which is the responsibility of the
 * caller.
 */
int
liquify_tpl_free_(LIQUIFYTPL *template)
{
	struct liquify_part *part, *nextpart;
	LIQUIFY *env;
	
	env = template->env;
	for(part = template->first; part; part = nextpart)
	{
		nextpart = part->next;
		liquify_tpl_free_part_(env, part);
	}
	liquify_free(env, template->name);
	liquify_free(env, template);
	return 0;
}

static int
liquify_tpl_free_part_(LIQUIFY *env, struct liquify_part *part)
{
	struct liquify_param *param, *nextparam;
	struct liquify_filter *filter, *nextfilter;
	
	switch(part->type)
	{
	case LPT_TEXT:
		liquify_free(env, part->d.text.text);
		break;
	case LPT_VAR:
		liquify_tpl_free_expr_contents_(env, &(part->d.var.expr));
		for(filter = part->d.var.ffirst; filter; filter = nextfilter)
		{
			nextfilter = filter->next;
			liquify_tpl_free_expr_contents_(env, &(filter->expr));
			for(param = filter->pfirst; param; param = nextparam)
			{
				nextparam = param->next;
				liquify_tpl_free_expr_contents_(env, &(param->expr));
				liquify_free(env, param);
			}
			liquify_free(env, filter);
		}
		break;
	case LPT_TAG:
		liquify_tpl_free_expr_contents_(env, &(part->d.tag.expr));
		for(param = part->d.tag.pfirst; param; param = nextparam)
		{
			nextparam = param->next;
			liquify_tpl_free_expr_contents_(env, &(param->expr));
			liquify_free(env, param);
		}
		break;
	}
	liquify_free(env, part);
	return 0;
}

static int
liquify_tpl_free_expr_contents_(LIQUIFY *env, struct liquify_expression *expr)
{
	liquify_tpl_free_token_(env, expr->root.left);
	liquify_tpl_free_token_(env, expr->root.right);
	return 0;
}

static int
liquify_tpl_free_token_(LIQUIFY *env, struct liquify_token *token)
{
	if(!token)
	{
		return 0;
	}
	liquify_tpl_free_token_(env, token->left);
	liquify_tpl_free_token_(env, token->right);
	liquify_free(env, token->text);
	liquify_free(env, token);
	return 0;
}

/* Add a new part to a template */
static struct liquify_part *
add_part(LIQUIFYTPL *tpl, int type)
{
	struct liquify_part *part;

	part = (struct liquify_part *) liquify_alloc(tpl->env, sizeof(struct liquify_part));
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
add_text(LIQUIFYTPL *tpl, int line, int col, const char *text, size_t len)
{
	struct liquify_part *part;

	part = add_part(tpl, LPT_TEXT);
	part->line = line;
	part->col = col;
	part->d.text.text = (char *) liquify_alloc(tpl->env, len + 1);
	strlcpy(part->d.text.text, text, len + 1);
	part->d.text.len = len;
	return part;
}

static struct liquify_filter *
add_filter(LIQUIFYTPL *tpl, struct liquify_part *part)
{
	struct liquify_filter *p;

	if(part->type != LPT_VAR)
	{
		return NULL;
	}
	p = (struct liquify_filter *) liquify_alloc(tpl->env, sizeof(struct liquify_filter));
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
add_filter_param(LIQUIFYTPL *tpl, struct liquify_filter *filter)
{
	struct liquify_param *p;

	p = (struct liquify_param *) liquify_alloc(tpl->env, sizeof(struct liquify_param));
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

static struct liquify_param *
add_tag_param(LIQUIFYTPL *tpl, struct liquify_part *tag)
{
	struct liquify_param *p;

	p = (struct liquify_param *) liquify_alloc(tpl->env, sizeof(struct liquify_param));
	if(tag->d.tag.pfirst)
	{
		tag->d.tag.plast->next = p;
	}
	else
	{
		tag->d.tag.pfirst = p;
	}
	tag->d.tag.plast = p;
	return p;
}

/* parse a filter, which has the form 'foo' or 'foo:"param1","param2"' */
static const char *
parse_filter(LIQUIFYTPL *tpl, struct liquify_part *part, struct liquify_filter *filter, const char *cur, int flags)
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
		PARTERRS(tpl, part, "expected: end-of-tag, vertical bar, or colon\n");
		return NULL;
	}
	if(expr->last->type == TOK_END || expr->last->type == TOK_VBAR)
	{
		return cur;
	}
	/* TOK_COLON - parameters follow */
	liquify_token_free_(tpl, expr->last);
	expr->last = NULL;
	while(tpl->pos < tpl->len)
	{
		param = add_filter_param(tpl, filter);
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
			PARTERRS(tpl, part, "expected: end-of-tag, vertical bar, or comma\n");
			return NULL;
		}
		if(expr->last->type == TOK_END || expr->last->type == TOK_VBAR)
		{
			filter->expr.last = expr->last;
			return cur;
		}
		/* TOK_COMMA - loop */
	}
	PARTERRS(tpl, part, "unexpected end of template while parsing variable output\n");
	return NULL;
}

/* Parse a 'variable' (output markup):
 *   {{ expr }}
 *   {{ expr | filter }}
 *   {{ expr | filter:"param","param","..." | filter2 }}
 */
static const char *
parse_var(LIQUIFYTPL *tpl, const char *cur)
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
	liquify_token_free_(tpl, expr->last);
	expr->last = NULL;
	while(tpl->pos < tpl->len)
	{
		if(t == TOK_END)
		{
			return cur;
		}
		if(t != TOK_VBAR)
		{
			PARTERRS(tpl, part, "expected: end-of-variable ('}}') or filter\n");
			return NULL;
		}
		filter = add_filter(tpl, part);
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
	PARTERRS(tpl, part, "unexpected end of template\n");
	return NULL;
}

/* Parse a tag:
 *  {% name %}
 *  {% name parameters... %}
 */
static const char *
parse_tag(LIQUIFYTPL *tpl, const char *cur)
{
	struct liquify_part *part;
	struct liquify_expression *expr;
	struct liquify_param *param;
	int finished, first;
	const char *ident;

	part = add_part(tpl, LPT_TAG);
	if(!part)
	{
		return NULL;
	}
	finished = 0;
	first = 1;
	cur += 2, tpl->pos += 2, tpl->col += 2;
	while(!finished && tpl->pos < tpl->len)
	{
		if(first)
		{
			expr = &(part->d.tag.expr);
			first = 0;
		}
		else
		{
			param = add_tag_param(tpl, part);
			expr = &(param->expr);
		}
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
				PARTERRS(tpl, part, "expected: end-of-tag ('%}') or parameters\n");
				return NULL;
			}
			liquify_token_free_(tpl, expr->last);
			expr->last = NULL;
		}
	}
	if(!finished)
	{
		PARTERRS(tpl, part, "expected end-of-tag ('%}'), but reached end of template\n");
		return NULL;
	}
	expr = &(part->d.tag.expr);
	if(!EXPR_IS(expr, TOK_IDENT))
	{
		PARTERRS(tpl, part, "expected: identifier at start of tag\n");
		return NULL;
	}
	ident = EXPR_IDENT(expr);
	if(!strncmp(ident, "end", 3))
	{
		if(liquify_is_block_(ident + 3))
		{
			if(!tpl->stack)
			{
				PARTERR(tpl, part, "unexpected {%% %s %%} outside of a block\n", ident);
				return NULL;
			}
			if(strcmp(EXPR_IDENT(&(tpl->stack->d.tag.expr)), ident + 3))
			{
				PARTERR(tpl, part, "unexpected {%% %s %}, expected {%% end%s %%}\n", ident, EXPR_IDENT(&(tpl->stack->d.tag.expr)));
				return NULL;
			}
			part->d.tag.kind = TPK_END;
			tpl->stack = tpl->stack->sprev;
			return cur;
		}
	}
	if(liquify_is_block_(ident))
	{
		part->d.tag.kind = TPK_BEGIN;
		if(liquify_block_parsed_(tpl, part, ident))
		{
			return NULL;
		}
		part->sprev = tpl->stack;
		tpl->stack = part;
		return cur;
	}
	if(liquify_is_tag_(ident))
	{
		part->d.tag.kind = TPK_TAG;
		if(liquify_tag_parsed_(tpl, part, ident))
		{
			return NULL;
		}
		return cur;
	}
	PARTERR(tpl, part, "expected: tag or block name, found '%s'\n", ident);
	return NULL;
}
