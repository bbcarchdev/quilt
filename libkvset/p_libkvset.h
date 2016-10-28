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

#ifndef P_LIBKVSET_H_
# define P_LIBKVSET_H_                 1

# include <stdio.h>

# include <stdlib.h>
# include <string.h>

# include "libkvset.h"

typedef struct kvset_entry_struct KVSETENTRY;

struct kvset_struct
{
	size_t count;
	KVSETENTRY *entries;
};

struct kvset_entry_struct
{
	char *key;
	size_t count;
	char **values;
};

KVSETENTRY *kvset_entry_locate_(KVSET *set, const char *key);
KVSETENTRY *kvset_entry_add_(KVSET *set, const char *key);

int kvset_value_add_(KVSETENTRY *entry, const char *value);
int kvset_value_reset_(KVSETENTRY *entry);

#endif /*!P_LIBKVSET_H_*/
