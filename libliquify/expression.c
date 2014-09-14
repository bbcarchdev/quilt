#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libliquify.h"

static int insert_token(struct liquify_expression *expr, struct liquify_token *token);
static jd_var *locate_var(struct liquify_token *tok, jd_var *dict, int vivify);

/* This is not a real expression parser - it reads a token, which must be an
 * identifier or a string literal, and will be followed by a terminator of
 * some kind (either an end-of-variable, end-of-tag, vertical bar, colon
 * or comma, depending upon 'flags'). However, if libliquify had a proper
 * expression parser, this is where it would go -- populating 'expr', and
 * returning a pointer to the first character after the expression in the
 * input buffer.
 */
const char *
liquify_expression_(LIQUIFYTPL *tpl, struct liquify_part *part, struct liquify_expression *expr, const char *cur, int flags)
{
	const char *start;
	int line, col, pos;

	if(!expr->cur)
	{
		expr->cur = &(expr->root);
	}
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
				PARTERRS(tpl, part, "expected: identifier or literal value");
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
		if(!expr->cur->right)
		{
			if(expr->last->type != TOK_IDENT)
			{
				PARTERRS(tpl, part, "expected: identifier");
				return NULL;
			}
			expr->cur->right = expr->last;
			continue;
		}
		if(expr->last->type == TOK_DOT)
		{
			if(expr->cur->right->type != TOK_IDENT)
			{
				PARTERRS(tpl, part, "object accessors can only follow identifiers");
				return NULL;
			}
			insert_token(expr, expr->last);
			continue;
		}
		/* Conditional tokens */
		if(expr->last->type == TOK_EQUALS || expr->last->type == TOK_NOTEQUALS)
		{
			insert_token(expr, expr->last);
			continue;
		}
		/* Not something we recognise as a valid continuation of an expression,
		 * so back-track to the beginning of the token
		 */
		liquify_token_free_(tpl, expr->last);
		expr->last = NULL;
		tpl->line = line;
		tpl->col = col;
		tpl->pos = pos;
		return start;
	}
	PARTERRS(tpl, part, "expected: expression");
	return NULL;
}

/* Evaluate an expression */
int
liquify_eval_(struct liquify_expression *expr, jd_var *dict, jd_var *dest, int vivify)
{
	jd_var *key;
	
	switch(expr->root.right->type)
	{
	case TOK_DOT:		
	case TOK_IDENT:
		key = locate_var(expr->root.right, dict, vivify);
		if(key)
		{
			jd_assign(dest, key);
		}
		return 0;
	case TOK_STRING:
		jd_set_string(dest, expr->root.right->text);
		return 0;
	}
	return -1;
}

/* Evaluate an expression to a boolean value */
int
liquify_eval_truth_(struct liquify_expression *expr, jd_var *dict)
{
	jd_var *key;

	switch(expr->root.right->type)
	{
	case TOK_DOT:
	case TOK_IDENT:
		key = locate_var(expr->root.right, dict, 0);
		if(!key)
		{
			return 0;
		}
		switch(key->type)
		{
		case VOID:
			return 0;
		case BOOL:
			return key->v.b;
		case INTEGER:
			return key->v.i == 0 ? 0 : 1;
		case REAL:
			return key->v.r == 0.0 ? 0 : 1;
		default:
			break;
		}
		return 1;
	case TOK_STRING:		
		return 1;
	}
	return 0;
}

/* Assign a value to an expression */
int
liquify_assign_(struct liquify_expression *expr, jd_var *dict, jd_var *newval)
{
	jd_var *key;
	
	switch(expr->root.right->type)
	{
	case TOK_IDENT:
		key = jd_get_ks(dict, expr->root.right->text, 1);
		jd_assign(key, newval);
		return 0;
	}
	return -1;
}

/* Push the current token down the tree */
static int
insert_token(struct liquify_expression *expr, struct liquify_token *tok)
{
	struct liquify_token save;
	
	save = *(expr->cur->right);
	*(expr->cur->right) = *tok;
	*tok = save;
	expr->cur->right->left = tok;
	expr->cur = expr->cur->right;
	return 0;
}

/* Locate an lvalue within dict based upon the contents of tok */
static jd_var *
locate_var(struct liquify_token *tok, jd_var *dict, int vivify)
{
	jd_var *left;

	if(!tok || !dict)
	{
		return NULL;
	}
	switch(tok->type)
	{
	case TOK_DOT:
		left = locate_var(tok->left, dict, 0);
		if(!left)
		{
			return NULL;
		}
		return locate_var(tok->right, left, vivify);
	case TOK_IDENT:
		return jd_get_ks(dict, tok->text, vivify);
	}
	return NULL;
}
