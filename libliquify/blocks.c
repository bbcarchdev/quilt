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

struct liquify_block_struct
{
	const char *name;
	int (*begin)(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack);
	int (*end)(LIQUIFYCTX *ctx, struct liquify_part *part, struct liquify_stack *stack);
	int (*cleanup)(LIQUIFYCTX *ctx, struct liquify_stack *stack);
};

#define BLOCK(name) \
	{ # name, liquify_block_ ## name ## _begin_, liquify_block_ ## name ## _end_, liquify_block_ ## name ## _cleanup_ }

static struct liquify_block_struct blocks[] = {
	BLOCK(for),
	BLOCK(if),
	{ NULL, NULL, NULL, NULL }
};


int
liquify_is_block_(const char *name)
{
	size_t c;

	for(c = 0; blocks[c].name; c++)
	{
		if(!strcmp(blocks[c].name, name))
		{
			return 1;
		}
	}
	return 0;
}

int
liquify_block_begin_(LIQUIFYCTX *ctx, struct liquify_part *part, const char *name, struct liquify_stack *stack)
{
	size_t c;

	for(c = 0; blocks[c].name; c++)
	{
		if(!strcmp(blocks[c].name, name))
		{
			return blocks[c].begin(ctx, part, stack);
		}
	}
	return -1;
}

int
liquify_block_end_(LIQUIFYCTX *ctx, struct liquify_part *part, const char *name, struct liquify_stack *stack)
{
	size_t c;

	for(c = 0; blocks[c].name; c++)
	{
		if(!strcmp(blocks[c].name, name))
		{
			return blocks[c].end(ctx, part, stack);
		}
	}
	return -1;
}

int
liquify_block_cleanup_(LIQUIFYCTX *ctx, const char *name, struct liquify_stack *stack)
{
	size_t c;

	for(c = 0; blocks[c].name; c++)
	{
		if(!strcmp(blocks[c].name, name))
		{
			return blocks[c].cleanup(ctx, stack);
		}
	}
	return -1;
}

