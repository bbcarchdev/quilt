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

KVSET *
kvset_create(void)
{
	KVSET *set;
	
	set = (KVSET *) calloc(1, sizeof(KVSET));
	if(!set)
	{
		return NULL;
	}
	return set;
}

int
kvset_destroy(KVSET *set)
{
	size_t c;
	
	for(c = 0; c < set->count; c++)
	{
		kvset_value_reset_(&(set->entries[c]));
	}
	free(set->entries);
	free(set);
	return 0;
}

