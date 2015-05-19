#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libliquify.h"

static int insert_token(struct liquify_expression *expr, struct liquify_token *token);
/* Locate or set a value in a tree of dictionaries */
static json_t *locate_var(struct liquify_token *tok, json_t *dict, json_t *newval);

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

/* Evaluate an expression; returns NULL if:
 * - an error occurred
 * - if newval was not specified, the expression did not evaluate to an object
 * otherwise, returns a new reference to an object which must be decremented.
 */
json_t *
liquify_eval_(struct liquify_expression *expr, json_t *dict, json_t *newval)
{
	switch(expr->root.right->type)
	{
	case TOK_DOT:
	case TOK_IDENT:
		return locate_var(expr->root.right, dict, newval);
	case TOK_STRING:
		return json_string(expr->root.right->text);
	}
	return NULL;
}

/* Evaluate an expression to a boolean value */
int
liquify_eval_truth_(struct liquify_expression *expr, json_t *dict)
{
	const char *t;
	json_t *value;
	int r;

	switch(expr->root.right->type)
	{
	case TOK_DOT:
	case TOK_IDENT:
		value = locate_var(expr->root.right, dict, NULL);
		if(!value)
		{
			return 0;
		}
		r = 0;
		switch(json_typeof(value))
		{
		case JSON_NULL:
		case JSON_FALSE:
			break;
		case JSON_TRUE:
			r = 1;
			break;
		case JSON_INTEGER:
			r = json_integer_value(value) == 0 ? 0 : 1;
			break;
		case JSON_REAL:
			r = json_real_value(value) == 0.0 ? 0 : 1;
			break;
		case JSON_STRING:
			t = json_string_value(value);
			if(t && t[0])
			{
				r = 1;
			}
			break;
		case JSON_ARRAY:
		case JSON_OBJECT:
			r = 1;
			break;
		default:
			break;
		}
		json_decref(value);
		return r;
	case TOK_STRING:
		return 1;
	}
	return 0;
}

/* Assign a value to an expression (currently non-hierarchical) */
int
liquify_assign_(struct liquify_expression *expr, json_t *dict, json_t *newval)
{
	switch(expr->root.right->type)
	{
	case TOK_IDENT:
		json_object_set(dict, expr->root.right->text, newval);
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

/* Locate an lvalue within dict based upon the contents of tok;
 * the result, if successful, is ALWAYS a new reference - even if it's
 * a copy of the pointer to newval
 */
static json_t *
locate_var(struct liquify_token *tok, json_t *dict, json_t *newval)
{
	json_t *left, *r;

	if(!tok || !dict)
	{
		return NULL;
	}
	switch(tok->type)
	{
	case TOK_DOT:
		left = locate_var(tok->left, dict, newval);
		if(!left)
		{
			return NULL;
		}
		r = locate_var(tok->right, left, newval);
		json_decref(left);
		return r;
	case TOK_IDENT:
		if(newval)
		{
			if(json_object_set(dict, tok->text, newval))
			{
				return NULL;
			}
			json_incref(newval);
			return newval;
		}
		newval = json_object_get(dict, tok->text);
		if(!newval)
		{
			return NULL;
		}
		json_incref(newval);
		return newval;
	}
	return NULL;
}
