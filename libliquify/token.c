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

#define HEXVAL(n) \
	(((n) >= '0' && (n) <= '9') ? (n) - '0' : \
		((n) >= 'a' && (n) <= 'f') ? (10 + (n) - 'a') : \
			(10 + (n) - 'A'))

#define ISOCTDIGIT(n) \
	((n) >= '0' && (n) <= '7')

static struct liquify_token *add_token(LIQUIFYTPL *tpl, struct liquify_expression *expr, int line, int col, int type, const char *text, size_t textlen);
static int copy_string(char *dest, const char *src, size_t srclen, int qmode);

/* Parse a single token and add it to an expression */
const char *
liquify_token_(LIQUIFYTPL *tpl, struct liquify_part *part, struct liquify_expression *expr, const char *cur, int flags)
{
	int q, e, line, col;
	const char *start;
	
	while(tpl->pos < tpl->len && isspace(*cur))
	{
		switch(*cur)
		{
		case 0:
		case '\r':
			tpl->pos++, cur++;
			break;
		case '\n':
		case '\f':
		case '\v':
			tpl->pos++, tpl->line++, cur++;
			tpl->col = 1;
			break;
		case '\t':
			tpl->pos++, cur++;
			tpl->col += TABSIZE;
			break;
		default:
			tpl->pos++, tpl->col++, cur++;
		}
	}
	if(tpl->pos >= tpl->len)
	{
		return NULL;
	}
	if(tpl->pos + 1 < tpl->len)
	{
		if(((flags & TKF_VAR) && cur[0] == '}' && cur[1] == '}') ||
			((flags & TKF_TAG) && cur[0] == '%' && cur[1] == '}'))
		{
			if(!add_token(tpl, expr, tpl->line, tpl->col, TOK_END, NULL, 0))
			{
				return NULL;
			}
			tpl->pos += 2, tpl->col += 2, cur += 2;
			return cur;
		}
	}
	if(*cur == '.')
	{
		if(!add_token(tpl, expr, tpl->line, tpl->col, TOK_DOT, NULL, 0))
		{
			return NULL;
		}
		tpl->pos++, tpl->col++, cur++;
		return cur;
	}
	if((flags & TKF_FILTERS) && *cur == '|')
	{
		if(!add_token(tpl, expr, tpl->line, tpl->col, TOK_VBAR, NULL, 0))
		{
			return NULL;
		}
		tpl->pos++, tpl->col++, cur++;
		return cur;
	}
	if((flags & TKF_COLON) && *cur == ':')
	{
		if(!add_token(tpl, expr, tpl->line, tpl->col, TOK_COLON, NULL, 0))
		{
			return NULL;
		}
		tpl->pos++, tpl->col++, cur++;
		return cur;
	}
	if((flags & TKF_COMMA) && *cur == ',')
	{
		if(!add_token(tpl, expr, tpl->line, tpl->col, TOK_COMMA, NULL, 0))
		{
			return NULL;
		}
		tpl->pos++, tpl->col++, cur++;
		return cur;
	}
	line = tpl->line;
	col = tpl->col;
	start = cur;
	if(*cur == '"' || *cur == '\'')
	{
		q = *cur;
		e = 0;
		/* Literal string */
		tpl->pos++, tpl->col++, cur++;
		while(tpl->pos < tpl->len)
		{
			if(!e && *cur == q)
			{
				tpl->pos++, tpl->col++, cur++;
				if(!add_token(tpl, expr, line, col, TOK_STRING, start, cur - start))
				{
					return NULL;
				}
				return cur;
			}
			switch(*cur)
			{
			case 0:
			case '\r':
				tpl->pos++, cur++;
				break;
			case '\n':
			case '\f':
			case '\v':
				tpl->pos++, tpl->line++, cur++;
				tpl->col = 1;
				break;
			case '\t':
				tpl->pos++, cur++;
				tpl->col += TABSIZE;
				break;
			case '\\':
				if(!e)
				{
					e = 1;
					tpl->pos++, tpl->col++, cur++;
					continue;
				}
			default:
				tpl->pos++, tpl->col++, cur++;
			}
			e = 0;
		}
		PARTERRS(tpl, part, "expected end of quoted literal but reached end-of-template");
		return NULL;
	}
	if(isalpha(*cur) || *cur == '_' || *cur == '$')
	{
		/* Identifier */
		while(tpl->pos < tpl->len && (isalnum(*cur) || *cur == '-' || *cur == '_' || *cur == '$'))
		{
			tpl->pos++, tpl->col++, cur++;
		}
		if(!add_token(tpl, expr, line, col, TOK_IDENT, start, cur - start))
		{
			return NULL;
		}
		return cur;
	}
	PARTERRS(tpl, part, "expected: expression");
	return NULL;
}

int
liquify_token_free_(LIQUIFYTPL *tpl, struct liquify_token *tok)
{
	if(tok)
	{
		liquify_free(tpl->env, tok->text);
		liquify_free(tpl->env, tok);
	}
	return 0;
}

/* Copy a quoted string from src to dest, processing any escape sequences */
static int
copy_string(char *dest, const char *src, size_t srclen, int qmode)
{
	int e, lval;
	const char *end;
	
	end = src + srclen - 1;
	src++;
	e = 0;
	while(src < end)
	{
		if(e)
		{
			e = 0;
			if(qmode)
			{
				/* Double quotes: process escape sequences */
				switch(*src)
				{
				case 'a':
					/* bell */
					*dest = '\a';
					break;
				case 'b':
					/* backspace */
					*dest = '\b';
					break;
				case 'f':
					/* form-feed */
					*dest = '\f';
					break;
				case 'n':
					/* new-line */
					*dest = '\n';
					break;
				case 'r':
					/* carriage return */
					*dest = '\r';
					break;
				case 't':
					/* tab */
					*dest = '\t';
					break;
				case 'v':
					/* vertical tab */
					*dest = '\v';
					break;
				case 'x':
					/* xNN - hexadecimal character */
					src++;
					if(src >= end || !isxdigit(*src))
					{
						*dest = '\\';
						dest++;
						*dest = 'x';
						dest++;
						continue;
					}
					lval = HEXVAL(*src);
					if(src + 1 < end && isxdigit(src[1]))
					{
						src++;
						lval <<= 4;
						lval |= HEXVAL(*src);
						src++;
					}
					*dest = lval;
					dest++;
					continue;
				default:
					if(ISOCTDIGIT(*src))
					{
						/* \NNN octal character literal */
						lval = (*src) - '0';
						src++;
						if(src < end && ISOCTDIGIT(*src))
						{
							lval <<= 3;
							lval |= (*src) - '0';
							src++;
							if(src < end && ISOCTDIGIT(*src))
							{
								lval <<= 3;
								lval |= (*src) - '0';
								src++;
							}
						}
						*dest = lval;
						dest++;
						continue;
					}
					*dest = *src;
				}
				dest++, src++;
				continue;
			}
			/* Single quotes: only recognise \\ and \' */
			if(*src == '\\' || *src == '\'')
			{
				*dest = *src;
				dest++, src++;
				continue;
			}
			*dest = '\\';
			dest++;
			*dest = *src;
			dest++, src++;
			continue;
		}
		if(*src == '\\')
		{
			e = 1;
			src++;
			continue;
		}
		/* Any other character */
		*dest = *src;
		dest++, src++;
	}
	return 0;
}

/* Add a token to an expression, optionally including a copy of the literal
 * text associated with it.
 */
static struct liquify_token *
add_token(LIQUIFYTPL *tpl, struct liquify_expression *expr, int line, int col, int type, const char *text, size_t textlen)
{
	struct liquify_token *p;
	
	p = (struct liquify_token *) liquify_alloc(tpl->env, sizeof(struct liquify_expression));
	p->line = line;
	p->col = col;
	p->type = type;
	if(text)
	{
		p->text = (char *) liquify_alloc(tpl->env, textlen+1);
		if(type == TOK_STRING)
		{
			copy_string(p->text, text, textlen, (*text == '"' ? 1 : 0));
		}
		else
		{
			strncpy(p->text, text, textlen);
			p->text[textlen] = 0;
		}
		p->len = textlen;
	}
	expr->last = p;
	return p;
}
