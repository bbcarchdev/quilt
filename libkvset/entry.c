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
kvset_add(KVSET *set, const char *key, const char *value)
{
	KVSETENTRY *entry;
	
	entry = kvset_entry_add_(set, key);
	if(!entry)
	{
		return -1;
	}
	return kvset_value_add_(entry, value);
}

int
kvset_set(KVSET *set, const char *key, const char *value)
{
	KVSETENTRY *entry;
	
	entry = kvset_entry_add_(set, key);
	if(!entry)
	{
		return -1;
	}
	if(kvset_value_reset_(entry) < 0)
	{
		return -1;
	}
	return kvset_value_add_(entry, value);
}

int
kvset_delete(KVSET *set, const char *key)
{
	KVSETENTRY *entry;
	
	entry = kvset_entry_locate_(set, key);
	if(!entry)
	{
		return 0;
	}
	kvset_value_reset_(entry);
	return 0;
}

const char *
kvset_get(KVSET *set, const char *key)
{
	KVSETENTRY *entry;
	
	entry = kvset_entry_locate_(set, key);
	if(!entry || !entry->count)
	{
		return NULL;
	}
	return entry->values[0];
}

const char * const *
kvset_getall(KVSET *set, const char *key)
{
	KVSETENTRY *entry;
	
	entry = kvset_entry_locate_(set, key);
	if(!entry || !entry->count)
	{
		return NULL;
	}
	return (const char *const *) entry->values;
}


KVSETENTRY *
kvset_entry_locate_(KVSET *set, const char *key)
{
	size_t c;
	
	for(c = 0; c < set->count; c++)
	{
		if(!strcmp(set->entries[c].key, key))
		{
			return &(set->entries[c]);
		}
	}
	return NULL;
}

KVSETENTRY *
kvset_entry_add_(KVSET *set, const char *key)
{
	KVSETENTRY *p;
	
	p = kvset_entry_locate_(set, key);
	if(p)
	{
		return p;
	}
	p = (KVSETENTRY *) realloc(set->entries, sizeof(KVSETENTRY) * (set->count + 1));
	if(!p)
	{
		return NULL;
	}
	set->entries = p;
	p = &(set->entries[set->count]);
	memset(p, 0, sizeof(KVSETENTRY));
	p->key = strdup(key);
	if(!p->key)
	{
		return NULL;
	}
	set->count++;
	return p;
}
