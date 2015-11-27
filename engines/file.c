/* file: reads RDF from Turtle files in a directory and serves them using
 * the Quilt serializers.
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

#include "p_file.h"

static int file_process(QUILTREQ *request);

static char *basepath;

# ifndef HAVE_STRLCPY
#  undef strlcpy
#  define strlcpy(dest, src, buflen)			\
	if(buflen > 1)								\
	{											\
		strncpy(dest, src, buflen - 1);			\
		(dest)[buflen - 1] = 0;					\
	}											\
	else if(buflen)								\
	{											\
		(dest)[0] = 0;							\
	}
#endif /*!HAVE_STRLCPY*/

# ifndef HAVE_STRLCAT
#  undef strlcat
#  define strlcat(dest, src, buflen)			\
	if(buflen > 1)								\
	{											\
		strncat(dest, src, buflen - 1);			\
		(dest)[buflen - 1] = 0;					\
	}											\
	else if(buflen)								\
	{											\
		(dest)[0] = 0;							\
	}
#endif /*!HAVE_STRLCAT*/

int
quilt_plugin_init(void)
{
	if(quilt_plugin_register_engine(QUILT_PLUGIN_NAME, file_process))
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to register engine\n");
		return -1;
	}
	basepath = quilt_config_geta("file:root", DATAROOTDIR "/" PACKAGE_TARNAME "/sample");
	return 0;
}

static int
file_process(QUILTREQ *request)
{
	librdf_world *world;
	librdf_parser *parser;
	librdf_model *model;
	librdf_uri *base;
	const char *s, *path, *basestr;
	char *pathname;
	size_t buflen, len;
	FILE *f;
	QUILTCANON *canonical;
	
	canonical = quilt_request_canonical(request);
	path = quilt_request_path(request);
	world = quilt_librdf_world();
	model = quilt_request_model(request);

	if(quilt_request_home(request))
	{
		s = "index";
	}
	else
	{
		s = path;
		quilt_canon_add_path(canonical, path);
	}
	/* QUILT-36: this will be unnecessary once the fragment is inferred */
	quilt_canon_set_fragment(canonical, "id");
	while(*s == '/')
	{
		s++;
	}
	len = strlen(basepath);
	/* basepath + '/' + s + '.ttl' */
	buflen = strlen(basepath) + 1 + strlen(s) + 4 + 1;
	pathname = (char *) malloc(buflen);
	if(!pathname)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate %u bytes\n", (unsigned) len);
		return 500;
	}
	strlcpy(pathname, basepath, buflen);
	pathname[len] = '/';
	pathname[len + 1] = 0;
	strlcat(pathname, s, buflen);
	strlcat(pathname, ".ttl", buflen);
	f = fopen(pathname, "rb");
	if(!f)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to open %s: %s\n", pathname, strerror(errno));
		free(pathname);
		return 404;
	}
	parser = librdf_new_parser(world, "turtle", NULL, NULL);
	if(!parser)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to create Turtle parser\n");
		fclose(f);
		free(pathname);
		return 404;
	}
	basestr = quilt_request_baseuristr(request);
	base = librdf_new_uri(world, (const unsigned char *) basestr);
	if(!base)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to create new RDF URI from <%s>\n", basestr);
		fclose(f);
		free(pathname);
		librdf_free_parser(parser);
		return 500;
	}
	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": parsing %s\n", pathname);
	if(librdf_parser_parse_file_handle_into_model(parser, f, 0, base, model))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to parse %s as Turtle\n", pathname);
		fclose(f);
		free(pathname);
		librdf_free_uri(base);
		librdf_free_parser(parser);
		return 503;
	}
	fclose(f);
	free(pathname);
	librdf_free_uri(base);
	librdf_free_parser(parser);
	/* Returning 200 (rather than 0) causes the model to be serialized
	 * automatically.
	 */
	return 200;
}
