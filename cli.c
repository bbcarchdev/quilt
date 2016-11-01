/* Quilt: Command-line query interface
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

#include "p_cli.h"

const char *quilt_progname = "quilt-cli";
static const char *query_string;

static int bulk, bulk_limit, bulk_offset;
static FILE *bulk_file;

/* Utilities */
static int process_args(int argc, char **argv);
static void usage(void);
static int config_defaults(void);
static int cli_process_(void);
static int cli_preprocess_(QUILTIMPLDATA *data);
static int cli_fallback_error_(QUILTIMPLDATA *data, int code);
static int cli_bulk_init_(QUILTREQ *request);

/* QUILTIMPL methods */
static const char *cli_getenv(QUILTREQ *request, const char *name);
static const char *cli_getparam(QUILTREQ *request, const char *name);
static int cli_put(QUILTREQ *request, const unsigned char *str, size_t len);
static int cli_vprintf(QUILTREQ *request, const char *format, va_list ap);
static int cli_header(QUILTREQ *request, const unsigned char *str, size_t len);
static int cli_headerf(QUILTREQ *request, const char *format, va_list ap);
static int cli_begin(QUILTREQ *request);
static int cli_end(QUILTREQ *request);

static QUILTIMPL cli_impl = {
	NULL, NULL, NULL,
	cli_getenv,
	cli_getparam,
	cli_put,
	cli_vprintf,
	cli_header,
	cli_headerf,
	cli_begin,
	cli_end
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

	t = getenv("QUILT_CONFIG");
	if(t)
	{
		config_set("global:configFile", t);
	}
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
	while((c = getopt(argc, argv, "hdc:t:bL:O:q:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(0);
		case 'd':
			config_set("log:level", "debug");
			config_set("log:stderr", "1");
			break;
		case 'c':
			config_set("global:configFile", optarg);
			break;
		case 't':
			setenv("HTTP_ACCEPT", optarg, 1);
			break;
		case 'b':
			bulk = 1;
			break;
		case 'L':
			bulk_limit = atoi(optarg);
			if(bulk_limit <= 0)
			{
				fprintf(stderr, "%s: '%s' is not a positive integer\n",
						quilt_progname, optarg);
				return -1;
			}
			break;
		case 'O':
			bulk_offset = atoi(optarg);
			if(bulk_offset <= 0)
			{
				fprintf(stderr, "%s: '%s' is not a positive integer\n",
						quilt_progname, optarg);
				return -1;
			}
			break;
		case 'q':
			query_string = optarg;
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if((bulk && argc) || (!bulk && argc != 1))
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
	fprintf(stderr, "Usage:\n"
			"  %s [OPTIONS] REQUEST-URI\n"
			"  %s -b [OPTIONS]\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -d                   Enable debug output\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -t TYPE              Specify MIME type to serialise as\n"
			"  -b                   Bulk-generate output\n"
			"  -L LIMIT             ... limiting to LIMIT items\n"
			"  -O OFFSET            ... starting from offset OFFSET\n"
			"  -q QUERY             Specify query parameters (key=value&key=value...)\n",
			quilt_progname, quilt_progname);
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
	int r, status;
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
		req = NULL;
	}
	else
	{
		if(bulk)
		{
			r = quilt_request_bulk(&cli_impl, data, bulk_offset, bulk_limit);
			req = NULL;
		}
		else
		{
			req = quilt_request_create(&cli_impl, data);
			if(!req)
			{
				r = -1;
			}
			else if((status = quilt_request_status(req)))
			{
				r = status;
			}
			else
			{
				r = quilt_request_process(req);
			}
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
	if(req)
	{
		quilt_request_free(req);
	}
	free(data->qbuf);
	data->qbuf = NULL;
	free(data->query);
	data->query = NULL;
	free(data);
	return 0;
}

static int
cli_preprocess_(QUILTIMPLDATA *data)
{
	const char *qs, *s, *t;
	char *p;
	char cbuf[3];
	size_t n;

	if(query_string)
	{
		qs = query_string;
	}
	else
	{
		qs = getenv("QUERY_STRING");
	}
	if(!qs)
	{
		data->qbuf = strdup("");
		data->query = (char **) calloc(1, sizeof(char *));
		return 0;
	}
	data->qbuf = (char *) calloc(1, strlen(qs) + 1);
	n = 1;
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
		p++;
		if(t)
		{
			t++;
		}
		s = t;
	}
	for(n = 0; data->query[n]; n++)
	{
		quilt_logf(LOG_DEBUG, "Query: [%s]\n", data->query[n]);
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
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	l = strlen(name);
	for(c = 0; data->query[c]; c++)
	{
		if(!strncmp(data->query[c], name, l) && data->query[c][l] == '=')
		{
			if(data->query[c][l + 1])
			{
				return &(data->query[c][l + 1]);
			}
			return NULL;
		}
	}
	return NULL;
}

static int
cli_put(QUILTREQ *request, const unsigned char *str, size_t len)
{
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	if(!data->headers_sent)
	{
		data->headers_sent = 1;
		if(bulk)
		{
			if(cli_bulk_init_(request))
			{
				return -1;
			}
		}
		else
		{
			fputc('\n', stdout);
		}
	}
	if(bulk)
	{
		if(!bulk_file)
		{
			return -1;
		}
		fwrite((void *) str, len, 1, bulk_file);
	}
	else
	{
		fwrite((void *) str, len, 1, stdout);
	}
	return 0;
}

static int
cli_vprintf(QUILTREQ *request, const char *format, va_list ap)
{
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	if(!data->headers_sent)
	{
		data->headers_sent = 1;
		if(bulk)
		{
			if(cli_bulk_init_(request))
			{
				return -1;
			}
		}
		else
		{
			fputc('\n', stdout);
		}
	}
	if(bulk)
	{
		if(!bulk_file)
		{
			return -1;
		}
		vfprintf(bulk_file, format, ap);
	}
	else
	{
		vprintf(format, ap);
	}
	return 0;
}

static int
cli_header(QUILTREQ *request, const unsigned char *str, size_t len)
{
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	if(!bulk)
	{
		if(data->headers_sent)
		{
			quilt_logf(LOG_WARNING, "cannot send headers; payload has already begun\n");
			return -1;
		}
		fwrite((void *) str, len, 1, stdout);
	}
	return 0;
}

static int
cli_headerf(QUILTREQ *request, const char *format, va_list ap)
{
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	if(!bulk)
	{
		if(data->headers_sent)
		{
			quilt_logf(LOG_WARNING, "cannot send headers; payload has already begun\n");
			return -1;
		}
		vprintf(format, ap);
	}
	return 0;
}

static int
cli_bulk_init_(QUILTREQ *request)
{
	char *path, *t, *p;
	int r;

	if(!bulk)
	{
		return 0;
	}
	path = quilt_canon_str(quilt_request_canonical(request), QCO_CONCRETE|QCO_NOABSOLUTE);
	t = path + 1;
	while(*t)
	{
		p = strchr(t, '/');
		if(!p)
		{
			break;
		}
		*p = 0;
		if(!path[1])
		{
			break;
		}
		do
		{
			r = mkdir(&(path[1]), 0777);
		}
		while(r == -1 && errno == EINTR);
		if(r == -1 && errno == EEXIST)
		{
			r = 0;
		}
		if(r)
		{
			quilt_logf(LOG_CRIT, "failed to create %s: %s\n", &(path[1]), strerror(errno));
			free(path);
			return -1;
		}
		*p = '/';
		t = p + 1;
	}
	bulk_file = fopen(&(path[1]), "wb");
	if(!bulk_file)
	{
		quilt_logf(LOG_CRIT, "failed to open %s for writing: %s\n", &(path[1]), strerror(errno));
		free(path);
		return -1;
	}
	free(path);
	return 0;
}

static int
cli_begin(QUILTREQ *request)
{
    QUILTIMPLDATA *data;

	data = quilt_request_impldata(request);
	data->headers_sent = 0;
	return 0;
}

static int
cli_end(QUILTREQ *request)
{
	(void) request;

	if(bulk_file)
	{
		fclose(bulk_file);
		bulk_file = NULL;
	}
	return 0;
}

