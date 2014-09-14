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

struct for_data
{
	jd_var list;
	jd_var keys;
	size_t idx;	
	struct liquify_expression *self;
};

static jd_var *for_current(LIQUIFYCTX *ctx, struct for_data *data);

int
liquify_block_for_begin_(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack)
{
	struct for_data *data;
	struct liquify_param *param;
	jd_var empty = JD_INIT;
	jd_var *v;

	if(stack->data)
	{
		data = (struct for_data *) stack->data;
	}
	else
	{
		data = (struct for_data *) calloc(1, sizeof(struct for_data));
		if(!data)
		{
			return -1;
		}		
		/* This is the first pass through the loop */
		param = part->d.tag.pfirst;
		if(liquify_assign_(&(param->expr), ctx->dict, &empty))
		{
			PARTERRS(ctx->tpl, part, "expected: lvalue as iterator");
			return -1;
		}
		data->self = &(param->expr);
		param = param->next;
		if(!param || !EXPR_IS(&(param->expr), TOK_IDENT) || strcmp(EXPR_IDENT(&(param->expr)), "in"))
		{
			PARTERRS(ctx->tpl, part, "expected: 'in'");
			return -1;
		}
		param = param->next;
		if(liquify_eval_(&(param->expr), ctx->dict, &(data->list), 0))
		{
			PARTERRS(ctx->tpl, part, "expected: identifier");
			return -1;
		}
		if(data->list.type == HASH)
		{
			jd_keys(&(data->keys), &(data->list));
		}
		stack->data = (void *) data;
	}
	v = for_current(ctx, data);		
	if(v)
	{
		liquify_assign_(data->self, ctx->dict, v);
	}
	else
	{		
		liquify_inhibit_(ctx);
	}
	return 0;
}

int
liquify_block_for_end_(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack)
{
	struct for_data *data;
	jd_var *v;
	int finished;

	(void) part;

	data = (struct for_data *) stack->data;
	data->idx++;
	finished = 1;
	JD_SCOPE
	{
		v = for_current(ctx, data);
		if(v)
		{
			finished = 0;
		}
		else
		{
			/* End of the loop */
		}
	}
	if(!finished)
	{
		liquify_goto_(ctx, stack->begin);
	}
	return 0;
}

int
liquify_block_for_cleanup_(LIQUIFYCTX *ctx, struct liquify_stack *stack)
{
	struct for_data *data;

	(void) ctx;

	data = (struct for_data *) stack->data;
	if(!data)
	{
		return 0;
	}
	stack->data = NULL;
	jd_release(&(data->keys));
	jd_release(&(data->list));
	free(data);
	return 0;
}
	  
static jd_var *
for_current(LIQUIFYCTX *ctx, struct for_data *data)
{
	jd_var *k, *v;
	size_t count;

	(void) ctx;

	if(data->keys.type == ARRAY)
	{
		count = jd_count(&(data->keys));
		if(count <= data->idx)
		{
			return NULL;
		}
		k = jd_get_idx(&(data->keys), count - data->idx - 1);
		if(!k)
		{
			return NULL;
		}
		v = jd_get_key(&(data->list), k, 0);
	}
	else if(data->list.type == ARRAY)
	{
		if(jd_count(&(data->list)) <= data->idx)
		{
			return NULL;
		}
		v = jd_get_idx(&(data->list), data->idx);
	}
	else
	{
		v = NULL;
	}
	return v;
}

