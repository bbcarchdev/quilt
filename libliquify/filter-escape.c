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
liquify_filter_escape_(LIQUIFYCTX *ctx, char *buf, size_t len, const char *name)
{
	char fbuf[16];
	size_t fblen;

	(void) name;

	fblen = 0;
	while(len)
	{
		if(*buf < 32 || *buf == '"' || *buf == '\'')
		{
			/* &#nn; output */
			if(fblen) liquify_emit(ctx, fbuf, fblen);
			fblen = 0;
			sprintf(fbuf, "&#%d;", (int) *buf);
			liquify_emit_str(ctx, fbuf);
		}
		else if(*buf == '&')
		{
			/* &amp; */
			if(fblen) liquify_emit(ctx, fbuf, fblen);
			fblen = 0;
			liquify_emit(ctx, "&amp;", 5);
		}
		else if(*buf == '<')
		{
			/* &lt; */
			if(fblen) liquify_emit(ctx, fbuf, fblen);
			fblen = 0;
			liquify_emit(ctx, "&lt;", 4);
		}
		else if(*buf == '>')
		{
			/* &gt */
			if(fblen) liquify_emit(ctx, fbuf, fblen);
			fblen = 0;
			liquify_emit(ctx, "&gt;", 4);
		}
		else
		{
			fbuf[fblen] = *buf;
			fblen++;
			if(fblen >= 15)
			{
				liquify_emit(ctx, fbuf, fblen);
				fblen = 0;
			}
		}
		len--;
		buf++;
	}
	if(fblen) liquify_emit(ctx, fbuf, fblen);
	return 0;
}
