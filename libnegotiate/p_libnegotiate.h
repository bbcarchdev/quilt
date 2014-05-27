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

#ifndef P_LIBNEGOTIATE_H_
# define P_LIBNEGOTIATE_H_             1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif

# include "libnegotiate.h"

struct negotiate_struct
{
	/* The list of supported entries */
	struct negotiate_entry_struct *entries;
	/* The number of entries in the entries array */
	size_t nentries;
	/* The matched entry, if any */
	struct negotiate_entry_struct *matched;
	/* The matched q-value */
	float q;
};

struct negotiate_entry_struct
{
	/* The name of the supported entry, e.g., 'text/plain' */
	char *name;
	/* The 'qs' (server-side quality) value, e.g., 0.75 */
	float qs;
	/* The wildcard q-value (i.e., *-slash-*) */
	float qw;
	/* The calculated q-value for a type-slash-* match */ 
	float qp;
	/* The calculated q-value for a specific match */
	float q;
};

#endif /*!P_LIBNEGOTIATE_H_*/
