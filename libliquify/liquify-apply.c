/* Parse a template, then apply a dictionary to it and output the result */

/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "libliquify.h"

static int parseargs(int argc, char **argv);
static void usage(void);
static char *readfile(const char *path);

static const char *short_program_name = "apply";
static const char *template_file;
static const char *dict_file;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] TEMPLATE JSON-DICT\n", short_program_name);
	fprintf(stderr, "OPTIONS is one or more of:\n");
	fprintf(stderr, "    -h             Print this usage message and exit\n");
}

static int
parseargs(int argc, char **argv)
{
	const char *t;
	int c;
	
	t = strrchr(argv[0], '/');
	if(t)
	{
		short_program_name = t + 1;
	}
	else
	{
		short_program_name = argv[0];
	}
	while((c = getopt(argc, argv, "h")) != -1)
	{
		switch(c)
		{
			case 'h':
				usage();
				exit(1);
			default:
				return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 2)
	{
		usage();
		exit(1);
	}
	template_file = argv[0];
	dict_file = argv[1];
	return 0;
}

static char *
readfile(const char *path)
{
	FILE *f;
	char *buf, *p;
	size_t len, alloc;
	ssize_t r;
	
	len = alloc = 0;
	buf = NULL;
	f = fopen(path, "rb");
	if(!f)
	{
		fprintf(stderr, "%s: %s: %s\n", short_program_name, path, strerror(errno));
		return NULL;
	}
	while(1)
	{
		p = (char *) realloc(buf, alloc + 512);
		if(!p)
		{
			fprintf(stderr, "%s: %s: %s\n", short_program_name, path, strerror(errno));
			free(buf);
			return NULL;
		}
		buf = p;
		alloc += 512;
		r = fread(&(buf[len]), 1, 511, f);
		if(r < 1)
		{
			break;
		}
		len += r;
	}
	buf[len] = 0;
	return buf;
}

int
main(int argc, char **argv)
{
	LIQUIFY *env;
	LIQUIFYTPL *template;
	char *buf;
	json_t *dict;
	json_error_t err;
	
	if(parseargs(argc, argv))
	{
		return 1;
	}
	
	buf = readfile(template_file);
	if(!buf)
	{
		return 1;
	}
	env = liquify_create();
	if(!env)
	{
		return 1;
	}
	template = liquify_parse(env, template_file, buf, strlen(buf));
	free(buf);
	if(!template)
	{
		fprintf(stderr, "*** %s: parse failed\n", template_file);
		return 1;
	}
	buf = readfile(dict_file);
	if(!buf)
	{
		return 1;
	}
	dict = json_loads(buf, 0, &err);
	free(buf);
	if(!dict)
	{
		fprintf(stderr, "%s:%d:%d: %s\n", dict_file, err.line, err.column, err.text);
		return 1;
	}
	buf = liquify_apply(template, dict);
	json_decref(dict);
	if(!buf)
	{
		fprintf(stderr, "*** %s: processing failed\n", template_file);
		return 1;
	}
	puts(buf);
	free(buf);
	return 0;
}
