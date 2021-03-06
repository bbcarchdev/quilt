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

int 
liquify_filter_downcase_(LIQUIFYCTX *ctx, char *buf, size_t len, const char *name)
{
	size_t c;

	(void) name;

	for(c = 0; c < len; c++)
	{
		buf[c] = tolower(buf[c]);
	}
	liquify_emit(ctx, buf, len);
	return 0;
}

int 
liquify_filter_upcase_(LIQUIFYCTX *ctx, char *buf, size_t len, const char *name)
{
	size_t c;

	(void) name;

	for(c = 0; c < len; c++)
	{
		buf[c] = toupper(buf[c]);
	}
	liquify_emit(ctx, buf, len);
	return 0;
}
