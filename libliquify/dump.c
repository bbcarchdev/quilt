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

static int dump_expression(struct liquify_expression *expr, FILE *f);
static int dump_token(struct liquify_token *token, FILE *f);
static int dump_text(const char *str, FILE *f);

int
liquify_dump(LIQUIFYTPL *tpl, FILE *f)
{
	struct liquify_part *part;
	struct liquify_filter *filter;
	struct liquify_param *param;
	
	for(part = tpl->first; part; part = part->next)
	{
		fprintf(f, "%s:%d:%d: ", tpl->name, part->line, part->col);
		switch(part->type)
		{
		case LPT_TEXT:
			fprintf(f, "string(%lu) = ", (unsigned long) part->d.text.len);
			dump_text(part->d.text.text, f);
			fputc('\n', f);
			break;
		case LPT_TAG:
			switch(part->d.tag.kind)
			{
			case TPK_TAG:
				fputs("tag: ", f);
				break;
			case TPK_BEGIN:
				fputs("block-begin: ", f);
				break;
			case TPK_END:
				fputs("block-end: ", f);
				break;
			}
			dump_expression(&(part->d.tag.expr), f);
			if(part->d.tag.pfirst)
			{
				fputs(" ( ", f);
				for(param = part->d.tag.pfirst; param; param = param->next)
				{
					if(param != part->d.tag.pfirst)
					{
						fputs(", ", f);
					}
					dump_expression(&(param->expr), f);
				}
				fputs(" )", f);
			}
			fputc('\n', f);
			break;
		case LPT_VAR:
			fprintf(f, "output: ");
			dump_expression(&(part->d.var.expr), f);
			for(filter = part->d.var.ffirst; filter; filter = filter->next)
			{
				fputs(" -> ", f);
				dump_expression(&(filter->expr), f);
				if(filter->pfirst)
				{
					fputs(" ( ", f);
					for(param = filter->pfirst; param; param = param->next)
					{
						if(param != filter->pfirst)
						{
							fputs(", ", f);
						}
						dump_expression(&(param->expr), f);
					}
					fputs(" )", f);
				}
			}
			fputc('\n', f);
			break;
		}
	}
	return 0;
}

static int
dump_text(const char *str, FILE *f)
{
	size_t c;
	
	if(!str)
	{
		fputs("(null)", f);
		return 0;
	}
	fputc('\'', f);
	if(isspace(*str))
	{
		fputs("...", f);
		while(isspace(*str))
		{
			str++;
		}
	}
	for(c = 0; *str && c < 16; c++)
	{
		if(*str == ' ' || *str == '\t' || !isspace(*str))
		{
			fputc(*str, f);
			str++;
		}
		else
		{
			fputs("...", f);
			break;
		}
	}
	if(c == 16)
	{
		fputs("...", f);
	}
	fputc('\'', f);
	return 0;
}

static int
dump_expression(struct liquify_expression *expr, FILE *f)
{
	dump_token(expr->root.right, f);
	return 0;
}

static int
dump_token(struct liquify_token *token, FILE *f)
{
	switch(token->type)
	{
	case TOK_NONE:
		fprintf(f, "TOK_NONE");
		break;
	case TOK_IDENT:
		fprintf(f, "TOK_IDENT(%lu) = %s", (unsigned long) token->len, token->text);
		break;
	case TOK_STRING:
		fprintf(f, "TOK_STRING(%lu) = ", (unsigned long) token->len);
		dump_text(token->text, f);
		break;
	case TOK_VBAR:
		fprintf(f, "TOK_VBAR");
		break;
	case TOK_END:
		fprintf(f, "TOK_END");
		break;
	case TOK_COLON:
		fprintf(f, "TOK_COLON");
		break;
	case TOK_COMMA:
		fprintf(f, "TOK_COMMA");
		break;
	}
	return 0;
}
