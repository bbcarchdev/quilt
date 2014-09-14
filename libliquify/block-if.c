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

int
liquify_tag_else_parsed_(LIQUIFYTPL *template, struct liquify_part *part)
{
	if(part->d.tag.pfirst)
	{
		PARTERRS(template, part, "unexpected expression following 'else'\n");
		return -1;
	}
	return 0;
}

int
liquify_tag_elsif_parsed_(LIQUIFYTPL *template, struct liquify_part *part)
{
	if(!part->d.tag.pfirst)
	{
		PARTERRS(template, part, "expected: conditional expression\n");
		return -1;
	}
	return 0;
}

int
liquify_tag_else_emit_(LIQUIFYCTX *ctx, struct liquify_part *part)
{
	struct if_data *data;
	
	if(!ctx->stack || strcmp(ctx->stack->ident, "if"))
	{
		PARTERRS(ctx->tpl, part, "unexpected 'else' outside of 'if'...'endif' block\n");
		return -1;
	}
	data = (struct if_data *) ctx->stack->data;
	if(data->matched)
	{
		if(!ctx->capture ||
		   !ctx->capture->inhibit ||
		   ctx->capture->owner != ctx->stack)
		{
			/* Inhibit output until the end of the block */
			liquify_inhibit_(ctx);
		}
		return 0;
	}
	if(!ctx->capture ||
	   !ctx->capture->inhibit ||
	   ctx->capture->owner != ctx->stack)
	{
		PARTERRS(ctx->tpl, part, "internal error: 'else' in unmatched branch while not inhibited or capture owner is not current stack head\n");
		return -1;
	}
	data->matched = 1;
	liquify_capture_end(ctx, NULL);
	return 0;
}

int
liquify_tag_elsif_emit_(LIQUIFYCTX *ctx, struct liquify_part *part)
{
	struct if_data *data;
	
	if(!ctx->stack || strcmp(ctx->stack->ident, "if"))
	{
		PARTERRS(ctx->tpl, part, "unexpected 'elsif' outside of 'if'...'endif' block\n");
		return -1;
	}
	data = (struct if_data *) ctx->stack->data;
	if(data->matched)
	{
		if(!ctx->capture ||
		   !ctx->capture->inhibit ||
		   ctx->capture->owner != ctx->stack)
		{
			/* Inhibit output until the end of the block */
			liquify_inhibit_(ctx);
		}
		return 0;
	}	
	/* The block didn't previously match a branch; if our expression
	 * is true, then end output inhibition; otherwise, leave everything
	 * unchanged.
	 */
	if(liquify_eval_truth_(&(part->d.tag.pfirst->expr), ctx->dict))
	{
		if(!ctx->capture ||
		   !ctx->capture->inhibit ||
		   ctx->capture->owner != ctx->stack)
		{
			PARTERRS(ctx->tpl, part, "internal error: 'else' in unmatched branch while not inhibited or capture owner is not current stack head\n");
			return -1;
		}
		data->matched = 1;
		liquify_capture_end(ctx, NULL);
	}
	return 0;
}
