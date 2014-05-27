/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

#ifndef LIBNEGOTIATE_H_
# define LIBNEGOTIATE_H_               1

typedef struct negotiate_struct NEGOTIATE;

/* Create a negotiation object */
NEGOTIATE *neg_create(void);
/* Destroy a negotiation object */
void neg_destroy(NEGOTIATE *neg);
/* Add a value and associated qs-value to the list */
int neg_add(NEGOTIATE *neg, const char *name, float qs);
/* Perform HTTP-style negotiation for a content type (i.e., a
 * a two-level value set -- such as text/plain, or audio/mpeg)
 */
const char *neg_negotiate_type(NEGOTIATE *neg, const char *accept);

#endif /*!LIBNEGOTIATE_H_*/