/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libquilt.h"

/* Internal: initialise libquilt */
int
quilt_init(quilt_log_fn logger, struct quilt_configfn_struct *fns)
{
	if(quilt_log_init_(logger))
	{
		return -1;
	}
	if(quilt_config_init_(fns))
	{
		return -1;
	}
	if(quilt_request_init_())
	{
		return -1;
	}
	if(quilt_librdf_init_())
	{
		return -1;
	}
	if(quilt_sparql_init_())
	{
		return -1;
	}
	if(quilt_plugin_init_())
	{
		return -1;
	}
	if(quilt_request_sanity_())
	{
		return -1;
	}
	return 0;
}
