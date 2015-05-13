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
	json_t *list;
	json_t *keys;
	size_t idx;	
	struct liquify_expression *self;
};

static json_t *for_current(LIQUIFYCTX *ctx, struct for_data *data);

/* Invoked when a new 'for' tag has been parsed */
int
liquify_block_for_parsed_(LIQUIFYTPL *tpl, struct liquify_part *part)
{
	struct liquify_param *param;

	param = part->d.tag.pfirst;
	if(!param)
	{
		PARTERRS(tpl, part, "expected: iterator variable name\n");
		return -1;
	}
	param = param->next;
	if(!param || !EXPR_IS(&(param->expr), TOK_IDENT) || strcmp(EXPR_IDENT(&(param->expr)), "in"))
	{
		PARTERRS(tpl, part, "expected: 'in'\n");
		return -1;
	}
	param = param->next;
	if(!param)
	{
		PARTERRS(tpl, part, "expected: object to iterate\n");
		return -1;
	}
	if(param->next)
	{
		PARTERRS(tpl, part, "unexpected tokens following iterator object\n");
		return -1;
	}
	return 0;
}

/* Invoked during template application to open a for loop */
int
liquify_block_for_begin_(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack)
{
	struct for_data *data;
	struct liquify_param *param;
	void *iter;
	json_t *v;

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
		data->self = &(param->expr);
		param = param->next;
		if(!param || !EXPR_IS(&(param->expr), TOK_IDENT) || strcmp(EXPR_IDENT(&(param->expr)), "in"))
		{
			PARTERRS(ctx->tpl, part, "expected: 'in'\n");
			return -1;
		}
		param = param->next;
		if(!(data->list = liquify_eval_(&(param->expr), ctx->dict, NULL)))
		{
			PARTERRS(ctx->tpl, part, "expected: identifier\n");
			return -1;
		}
		if(json_typeof(data->list) == JSON_OBJECT)
		{
			data->keys = json_array();
			for(iter = json_object_iter(data->list); iter; iter = json_object_iter_next(data->list, iter))
			{
				json_array_append_new(data->keys, json_string(json_object_iter_key(iter)));
			}
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
	json_t *v;
	int finished;

	(void) part;

	data = (struct for_data *) stack->data;
	data->idx++;
	finished = 1;
	v = for_current(ctx, data);
	if(v)
	{
		finished = 0;
	}
	else
	{
		/* End of the loop */
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
	if(data->keys)
	{
		json_decref(data->keys);
	}
	if(data->list)		
	{
		json_decref(data->list);
	}
	free(data);
	return 0;
}

/* Return a BORROWED reference to the current value in a for loop */	  
static json_t *
for_current(LIQUIFYCTX *ctx, struct for_data *data)
{
	json_t *k, *v;
	size_t count;

	(void) ctx;

	if(data->keys && json_typeof(data->keys) == JSON_ARRAY)
	{
		count = json_array_size(data->keys);
		if(count <= data->idx)
		{
			return NULL;
		}
		k = json_array_get(data->keys, count - data->idx - 1);
		if(!k)
		{
			return NULL;
		}
		if(json_typeof(k) != JSON_STRING)
		{
			return NULL;
		}
		v = json_object_get(data->list, json_string_value(k));
	}
	else if(json_typeof(data->list) == JSON_ARRAY)
	{
		if(json_array_size(data->list) <= data->idx)
		{
			return NULL;
		}
		v = json_array_get(data->list, data->idx);
	}
	else
	{
		v = NULL;
	}
	return v;
}

