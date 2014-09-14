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

struct liquify_tag_struct
{
	const char *name;
	int (*parsed)(LIQUIFYTPL *template, struct liquify_part *part);
	int (*emit)(LIQUIFYCTX *ctx, struct liquify_part *part);
};

#define TAG(name) \
	{ # name, liquify_tag_ ## name ## _parsed_, liquify_tag_ ## name ## _ }

static struct liquify_tag_struct tags[] = {
	TAG(include),
	TAG(else),
	TAG(elsif),
	{ NULL, NULL, NULL }
};

int
liquify_is_tag_(const char *name)
{
	size_t c;

	for(c = 0; tags[c].name; c++)
	{
		if(!strcmp(tags[c].name, name))
		{
			return 1;
		}
	}
	return 0;
}

int
liquify_tag_parsed_(LIQUIFYTPL *template, struct liquify_part *part, const char *name)
{
	size_t c;

	for(c = 0; tags[c].name; c++)
	{
		if(!strcmp(tags[c].name, name))
		{
			return tags[c].parsed(template, part);
		}
	}
	return -1;
}

int
liquify_tag_(LIQUIFYCTX *ctx, struct liquify_part *part, const char *name)
{
	size_t c;

	for(c = 0; tags[c].name; c++)
	{
		if(!strcmp(tags[c].name, name))
		{
			return tags[c].emit(ctx, part);
		}
	}
	return -1;
}

