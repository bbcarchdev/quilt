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

char *html_baseuri;
size_t html_baseurilen;

static int html_serialize(QUILTREQ *req);

/* Quilt plug-in entry-point */
int
quilt_plugin_init(void)
{
	size_t c;

	html_baseuri = quilt_config_geta("quilt:base", NULL);
	if(!html_baseuri)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to determine base URI from configuration\n");
		return -1;
	}
	html_baseurilen = strlen(html_baseuri);
	if(html_template_init())
	{
		free(html_baseuri);
		html_baseuri = NULL;
		return -1;
	}
	for(c = 0; html_types[c].mimetype; c++)
	{
		quilt_plugin_register_serializer(&(html_types[c]), html_serialize);
	}
	return 0;
}

static int
html_serialize(QUILTREQ *req)
{
	QUILTCANON *canon;
	LIQUIFYTPL *tpl;
	json_t *dict;
	char *buf, *loc;
	int status;
	QUILTCANOPTS opt = QCO_CONCRETE|QCO_NOABSOLUTE;

	status = 500;
	canon = quilt_request_canonical(req);
	dict = json_object();
	html_add_common(dict, req);
	html_add_request(dict, req);
	html_add_model(dict, req);
	tpl = html_template(req);
	if(tpl)
	{
		/* Set status to zero to suppress output */
		status = 0;
		buf = liquify_apply(tpl, dict);
		if(quilt_request_status(req) > 299)
		{
			// During error, we might need to use user supplied path, SPINDLE#66
			opt |= QCO_USERSUPPLIED;
		}
		loc = quilt_canon_str(canon, opt);
		quilt_request_headerf(req, "Status: %d %s\n", quilt_request_status(req), quilt_request_statustitle(req));
		quilt_request_headerf(req, "Content-Type: %s; charset=utf-8\n", quilt_request_type(req));
		quilt_request_headerf(req, "Content-Location: %s\n", loc);
		quilt_request_headers(req, "Vary: Accept\n");
		quilt_request_headers(req, "Server: " PACKAGE_SIGNATURE "\n");
		free(loc);
		quilt_request_puts(req, buf);
		free(buf);
	}
	json_decref(dict);
	return status;
}
