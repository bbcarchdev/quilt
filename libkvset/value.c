/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2016 BBC.
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

#include "p_libkvset.h"

int
kvset_value_add_(KVSETENTRY *entry, const char *value)
{
	char **p;
	char *s;
	
	s = strdup(value);
	if(!s)
	{
		return -1;
	}
	p = (char **) realloc(entry->values, sizeof(char *) * (entry->count + 2));
	if(!p)
	{
		return -1;
	}
	entry->values = p;
	entry->values[entry->count] = s;
	entry->count++;
	entry->values[entry->count] = NULL;
	return 0;
}

int
kvset_value_reset_(KVSETENTRY *entry)
{
	size_t c;
	
	for(c = 0; c < entry->count; c++)
	{
		free(entry->values[c]);
	}
	free(entry->values);
	entry->values = NULL;
	entry->count = 0;
	return 0;
}