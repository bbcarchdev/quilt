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

struct if_data
{
	/* Set once a branch has been processed, inhibiting any further
	 * 'else' blocks
	 */
	int matched;
};

int
liquify_block_if_begin_(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack)
{
	struct if_data *data;
	struct liquify_param *param;

	data = (struct if_data *) liquify_alloc(ctx->tpl->env, sizeof(struct if_data));
	stack->data = (void *) data;
	param = part->d.tag.pfirst;
	if(!param)
	{
		PARTERRS(ctx->tpl, part, "expected: conditional expression");
		return -1;
	}
	if(liquify_eval_truth_(&(param->expr), ctx->dict))
	{
		data->matched = 1;
	}
	else
	{
		liquify_inhibit_(ctx);
	}
	return 0;
}

int
liquify_block_if_end_(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack)
{
	(void) ctx;
	(void) part;
	(void) stack;
	
	return 0;
}

int
liquify_block_if_cleanup_(LIQUIFYCTX *ctx, struct liquify_stack *stack)
{
	liquify_free(ctx->tpl->env, stack->data);

	return 0;
}

