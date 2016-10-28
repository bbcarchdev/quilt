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

#ifndef LIBKVSET_H_
# define LIBKVSET_H_                   1

typedef struct kvset_struct KVSET;

KVSET *kvset_create(void);
int kvset_destroy(KVSET *set);
int kvset_add(KVSET *set, const char *key, const char *value);
int kvset_set(KVSET *set, const char *key, const char *value);
int kvset_delete(KVSET *set, const char *key);
const char *kvset_get(KVSET *set, const char *key);
const char * const *kvset_getall(KVSET *set, const char *key);

#endif /*!LIBKVSET_H_*/
