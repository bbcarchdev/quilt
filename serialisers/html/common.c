/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

#include "p_html.h"

/* Add common information to the dictionary */
int
html_add_common(json_t *dict, QUILTREQ *req)
{
	json_t *r;

	(void) req;

	r = json_object();
	json_object_set_new(r, "title", json_string(PACKAGE_TITLE));
	json_object_set_new(r, "name", json_string(PACKAGE_NAME));
	json_object_set_new(r, "version", json_string(PACKAGE_VERSION));
	json_object_set_new(r, "url", json_string(PACKAGE_URL));
	json_object_set_new(r, "signature", json_string(PACKAGE_SIGNATURE));
	json_object_set_new(dict, "package", r);
	return 0;
}
