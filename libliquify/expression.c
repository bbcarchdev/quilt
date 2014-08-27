#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libliquify.h"

/* This is not a real expression parser - it reads a token, which must be an
 * identifier or a string literal, and will be followed by a terminator of
 * some kind (either an end-of-variable, end-of-tag, vertical bar, colon
 * or comma, depending upon 'flags'). However, if libliquify had a proper
 * expression parser, this is where it would go -- populating 'expr', and
 * returning a pointer to the first character after the expression in the
 * input buffer.
 */
const char *
liquify_expression_(struct liquify_template *tpl, struct liquify_part *part, struct liquify_expression *expr, const char *cur, int flags)
{
	const char *start;
	int line, col, pos;
	
	while(tpl->pos < tpl->len)
	{
		start = cur;
		line = tpl->line;
		col = tpl->col;
		pos = tpl->pos;
		cur = liquify_token_(tpl, part, expr, cur, flags);
		if(!cur)
		{
			return NULL;
		}
		if(!expr->root.right)
		{
			if(expr->last->type != TOK_IDENT && expr->last->type != TOK_STRING)
			{
				liquify_parse_error_(tpl, part, "expected: identifier or literal value");
				return NULL;
			}
			expr->root.right = expr->last;
			continue;
		}
		if(expr->last->type == TOK_END ||
			(flags & TKF_FILTERS && expr->last->type == TOK_VBAR) ||
			(flags & TKF_COLON && expr->last->type == TOK_COLON) ||
			(flags & TKF_COMMA && expr->last->type == TOK_COMMA))
		{
			return cur;
		}
		/* Not something we recognise as a valid continuation of an expression,
		 * so back-track to the beginning of the token
		 */
		liquify_token_free_(expr->last);
		expr->last = NULL;
		tpl->line = line;
		tpl->col = col;
		tpl->pos = pos;
		return start;
	}
	liquify_parse_error_(tpl, part, "expected: expression");
	return NULL;
}

/* Evaluate an expression */
int
liquify_eval_(struct liquify_expression *expr, jd_var *dict, jd_var **dest)
{
	jd_var *key;
	
	switch(expr->root.right->type)
	{
	case TOK_IDENT:
		key = jd_get_ks(dict, expr->root.right->text, 0);
		if(key)
		{
			jd_retain(key);
			*dest = key;
		}
		return 0;
	case TOK_STRING:
		*dest = jd_nv();
		jd_retain(*dest);
		jd_set_string(*dest, expr->root.right->text);
		return 0;
	}
	return -1;
}
