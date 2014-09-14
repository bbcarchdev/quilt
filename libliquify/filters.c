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

struct liquify_filter_struct
{
	const char *name;
	int (*fn)(LIQUIFYCTX *ctx, char *buf, size_t len, const char *name);
};

#define FILTER(name) \
	{ # name, liquify_filter_ ## name ## _ }

static struct liquify_filter_struct filters[] = {
	FILTER(escape),
	FILTER(downcase),
	FILTER(upcase),
	{ NULL, NULL }
};

int
liquify_is_filter_(const char *name)
{
	size_t c;

	for(c = 0; filters[c].name; c++)
	{
		if(!strcmp(name, filters[c].name))
		{
			return 1;
		}
	}
	return 0;
}

int
liquify_filter_apply_(LIQUIFYCTX *ctx, const char *name, char *buf, size_t len)
{
	size_t c;

	for(c = 0; filters[c].name; c++)
	{
		if(!strcmp(name, filters[c].name))
		{
			return filters[c].fn(ctx, buf, len, name);
		}
	}
	liquify_emit_str(ctx, "[Warning: no such filter '");
	liquify_emit_str(ctx, name);
	liquify_emit_str(ctx, "]");
	return -1;
}

