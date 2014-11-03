/* Quilt: Command-line query interface
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

/* This is the FastCGI interface for Quilt; it can be used with any
 * FastCGI-compatible server, either in standalone or on-demand modes
 * (i.e., it can be launched as a daemon which opens a FastCGI socket,
 * or it can be invoked by a web server as needed).
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_cli.h"

const char *quilt_progname = "quilt-cli";

/* Utilities */
static int process_args(int argc, char **argv);
static void usage(void);
static int config_defaults(void);
static int cli_process_(void);
static int cli_preprocess_(QUILTIMPLDATA *data);
static int cli_fallback_error_(QUILTIMPLDATA *data, int code);

/* QUILTIMPL methods */
static const char *cli_getenv(QUILTREQ *request, const char *name);
static const char *cli_getparam(QUILTREQ *request, const char *name);
static int cli_put(QUILTREQ *request, const char *str, size_t len);
static int cli_printf(QUILTREQ *request, const char *format, ...);
static int cli_vprintf(QUILTREQ *request, const char *format, va_list ap);

static QUILTIMPL cli_impl = {
	NULL, NULL, NULL,
	cli_getenv,
	cli_getparam,
	cli_put,
	cli_printf,
	cli_vprintf,
};

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
	if(quilt_init(log_vprintf, &configfn))
	{
		return 1;
	}
	if(cli_process_())
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
	setenv("HTTP_ACCEPT", "text/turtle", 1);
	setenv("REQUEST_METHOD", "GET", 1);
	while((c = getopt(argc, argv, "hDc:t:")) != -1)
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
		case 't':
			setenv("HTTP_ACCEPT", optarg, 1);
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 1)
	{
		usage();
		return -1;
	}
	setenv("REQUEST_URI", argv[0], 1);
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] REQUEST-URI\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -D                   Enable debug output\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -t TYPE              Specify MIME type to serialise as\n",
			quilt_progname);
}

static int
config_defaults(void)
{
	config_set_default("global:configFile", SYSCONFDIR "/quilt.conf");
	config_set_default("log:level", "notice");
	config_set_default("log:facility", "user");
	config_set_default("log:syslog", "0");
	config_set_default("log:stderr", "1");
	config_set_default("sparql:query", "http://localhost/sparql/");
	config_set_default("fastcgi:socket", "/tmp/quilt.sock");	
	config_set_default("quilt:base", "http://www.example.com/");
	return 0;
}

static int
cli_process_(void)
{
	int r;
	QUILTIMPLDATA *data;
	QUILTREQ *req;
		
	data = (QUILTIMPLDATA *) calloc(1, sizeof(QUILTIMPLDATA));
	if(!data)
	{
		log_printf(LOG_CRIT, "failed to allocate memory for command-line request\n");
		return -1;
	}
	if(cli_preprocess_(data))
	{
		r = -1;
	}
	else
	{
		req = quilt_request_create(&cli_impl, data);
		if(!req)
		{
			r = -1;
		}
		else if(req->status)
		{
			r = req->status;
		}
		else
		{
			r = quilt_request_process(req);
		}
	}
	if(r < 0)
	{
		r = 500;
	}
	if(r)
	{
		if(req)
		{
			quilt_error(req, r);
		}
		else
		{
			cli_fallback_error_(data, r);
		}
	}
	quilt_request_free(req);
	free(data->qbuf);
	data->qbuf = NULL;
	data->query = NULL;
	return 0;
}

static int
cli_preprocess_(QUILTIMPLDATA *data)
{
	const char *qs, *s, *t;
	char *p;
	char cbuf[3];
	size_t n;

	qs = getenv("QUERY_STRING");
	if(!qs)
	{
		data->qbuf = strdup("");
		data->query = (char **) calloc(1, sizeof(char *));
		return 0;
	}
	data->qbuf = (char *) calloc(1, strlen(qs) + 1);
	n = 0;
	s = qs;
	while(s)
	{
		t = strchr(s, '&');
		if(t)
		{
			t++;
			n++;
		}
		s = t;
	}
	data->query = (char **) calloc(n + 1, sizeof(char *));
	p = data->qbuf;
	s = qs;
	n = 0;
	while(s)
	{
		data->query[n] = p;
		n++;
		t = strchr(s, '&');
		while(*s &&(!t || s < t))
		{
			if(*s == '%')
			{
				if(isxdigit(s[1])  && isxdigit(s[2]))
				{
					cbuf[0] = s[1];
					cbuf[1] = s[2];
					cbuf[2] = 0;
					*p = (char) ((unsigned char) strtol(cbuf, NULL, 16));
					p++;
					s += 3;
					continue;
				}
			}
			*p = *s;
			p++;
			s++;
		}
		*p = 0;
		if(t)
		{
			t++;
		}
		s = t;
	}		
	return 0;
}

static
int cli_fallback_error_(QUILTIMPLDATA *data, int status)
{
	(void) data;

	fprintf(stderr, "%s: response status %d\n", quilt_progname, status);
	return 0;
}

/* QUILTIMPL methods */
static const char *
cli_getenv(QUILTREQ *request, const char *name)
{
	(void) request;

	return getenv(name);
}

static const char *
cli_getparam(QUILTREQ *request, const char *name)
{
	size_t c, l;
       
	l = strlen(name);
	for(c = 0; request->data->query[c]; c++)
	{
		if(!strncmp(request->data->query[c], name, l) && request->data->query[c][l] == '-')
		{
			return &(request->data->query[c][l + 1]);
		}
	}
	return NULL;
}

static int
cli_put(QUILTREQ *request, const char *str, size_t len)
{
	(void) request;

	fwrite(str, len, 1, stdout);
	return 0;
}

static int
cli_printf(QUILTREQ *request, const char *format, ...)
{
	va_list ap;

	(void) request;

	va_start(ap, format);
	return vprintf(format, ap);
}

static int
cli_vprintf(QUILTREQ *request, const char *format, va_list ap)
{
	(void) request;

	return vprintf(format, ap);
}