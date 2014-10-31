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

#include "p_quilt.h"

const char *quilt_progname = "quiltd";

static int process_args(int argc, char **argv);
static void usage(void);
static int config_defaults(void);

int
main(int argc, char **argv)
{
	struct quilt_configfn_struct configfn;
    
	log_set_ident(argv[0]);
	log_set_stderr(1);
	log_set_level(LOG_NOTICE);
	if(config_init(config_defaults))
	{
		return 1;
	}
	if(process_args(argc, argv))
	{
		return 1;
	}
	if(config_load(NULL))
	{
		return 1;
	}
	log_set_use_config(1);
	configfn.config_get = config_get;
	configfn.config_geta = config_geta;
	configfn.config_get_int = config_get_int;
	configfn.config_get_bool = config_get_bool;
	configfn.config_get_all = config_get_all;
	if(quilt_init_(log_vprintf, &configfn))
	{
		return 1;
	}
	/* XXX Daemonize */
	if(fcgi_init())
	{
		return 1;
	}
/*	if(quilt_html_init())
	{
		return 1;
	} */
	if(fcgi_runloop())
	{
		return 1;
	}
	return 0;
}

static int
process_args(int argc, char **argv)
{
	const char *t;
	int c;

	if(argc > 0 && argv[0])
	{
		t = strrchr(argv[0], '/');
		if(t)
		{
			quilt_progname = t + 1;
		}
		else
		{
			quilt_progname = argv[0];
		}
	}
	config_set_default("log:ident", quilt_progname);
	while((c = getopt(argc, argv, "hDc:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(0);
		case 'D':
			config_set("log:level", "debug");
			config_set("log:stderr", "1");
			break;
		case 'c':
			config_set("global:configFile", optarg);
			break;
		default:
			usage();
			return -1;
		}
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -D                   Enable debug output to standard error\n"
			"  -c FILE              Specify path to configuration file\n",
			quilt_progname);
}

static int
config_defaults(void)
{
	config_set_default("global:configFile", SYSCONFDIR "/quilt.conf");
	config_set_default("log:level", "notice");
	config_set_default("log:facility", "daemon");
	config_set_default("log:syslog", "1");
	config_set_default("log:stderr", "0");
	config_set_default("sparql:query", "http://localhost/sparql/");
	config_set_default("fastcgi:socket", "/tmp/quilt.sock");	
	config_set_default("quilt:base", "http://www.example.com/");
	return 0;
}
