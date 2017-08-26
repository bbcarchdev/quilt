/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

#ifndef P_JSONLD_H_
# define P_JSONLD_H_                    1

# define _BSD_SOURCE                    1

# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <librdf.h>
# include <rdf_list.h>
# include <jansson.h>

# include "libquilt.h"

# define QUILT_PLUGIN_NAME              "jsonld"

# define NS_RDF                         "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
# define NS_XSD                         "http://www.w3.org/2001/XMLSchema#"

/* Debian Wheezy ships with libjansson 2.3, which doesn't include
 * json_array_foreach()
 */
# ifndef json_array_foreach
#  define json_array_foreach(array, index, value) \
	for(index = 0; index < json_array_size(array) && (value = json_array_get(array, index)); index++)
# endif

#endif /*!P_JSONLD_H_*/
