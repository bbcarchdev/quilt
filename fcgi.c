/* Quilt: FastCGI server interface
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

/* This is the FastCGI interface for Quilt; it can be used with any
 * FastCGI-compatible server, either in standalone or on-demand modes
 * (i.e., it can be launched as a daemon which opens a FastCGI socket,
 * or it can be invoked by a web server as needed).
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_fcgi.h"

const char *quilt_progname = "quilt-fcgid";
static URI *quilt_fcgi_uri;
static int quilt_fcgi_socket = -1;

/* Utilities */
static int process_args(int argc, char **argv);
static void usage(void);
static int config_defaults(void);
static int fcgi_init_(void);
static int fcgi_runloop_(void);
static int fcgi_sockpath_(URI *uri, char **ptr);
static int fcgi_hostport_(URI *uri, char **ptr);
static int fcgi_preprocess_(QUILTIMPLDATA *data);
static int fcgi_fallback_error_(QUILTIMPLDATA *data, int code);

/* QUILTIMPL methods */
static const char *fcgi_getenv(QUILTREQ *request, const char *name);
static const char *fcgi_getparam(QUILTREQ *request, const char *name);
static int fcgi_put(QUILTREQ *request, const unsigned char *str, size_t len);
static int fcgi_vprintf(QUILTREQ *request, const char *format, va_list ap);
static int fcgi_header(QUILTREQ *request, const unsigned char *str, size_t len);
static int fcgi_headerf(QUILTREQ *request, const char *format, va_list ap);
static int fcgi_begin(QUILTREQ *request);
static int fcgi_end(QUILTREQ *request);

static QUILTIMPL fcgi_impl = {
	NULL, NULL, NULL,
	fcgi_getenv,
	fcgi_getparam,
	fcgi_put,
	fcgi_vprintf,
	fcgi_header,
	fcgi_headerf,
	fcgi_begin,
	fcgi_end
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
	if(fcgi_init_())
	{
		return 1;
	}
	if(fcgi_runloop_())
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
	while((c = getopt(argc, argv, "hdc:")) != -1)
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
			"  -d                   Enable debug output to standard error\n"
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

static int
fcgi_init_(void)
{
	char *p;
	int ispath;
	struct stat statbuf;

	if(FCGX_Init())
	{
		return -1;
	}	
	if(0 == fstat(0, &statbuf) && S_ISSOCK(statbuf.st_mode))
	{
		log_printf(LOG_DEBUG, "invoked by FastCGI web server; will not open new listening socket\n");
		quilt_fcgi_socket = 0;
	}
	else
	{
		p = config_geta("fastcgi:socket", NULL);
		if(!p)
		{
			log_printf(LOG_CRIT, "failed to retrieve FastCGI socket URI from configuration\n");
			return -1;
		}	
		quilt_fcgi_uri = uri_create_str(p, NULL);
		if(!quilt_fcgi_uri)
		{
			log_printf(LOG_CRIT, "failed to parse <%s>\n", p);
			free(p);
			return -1;
		}
		free(p);
		p = NULL;
		if(fcgi_sockpath_(quilt_fcgi_uri, &p) == -1)
		{
			return -1;
		}
		if(p)
		{
			ispath = 1;
		}
		else
		{
			ispath = 0;
			if(fcgi_hostport_(quilt_fcgi_uri, &p) == -1)
			{
				return -1;
			}
			if(!p)
			{
				log_printf(LOG_ERR, "failed to obtain either a socket path or host:port from FastCGI URI");
				return -1;
			}
		}
		log_printf(LOG_DEBUG, "opening FastCGI socket %s\n", p);   
		quilt_fcgi_socket = FCGX_OpenSocket(p, 5);
		if(ispath)
		{
			chmod(p, 0777);
		}
		free(p);
	}
	if(quilt_fcgi_socket < 0)
	{
		log_printf(LOG_ERR, "failed to open FastCGI socket: %s\n", p);
		free(p);
		return -1;
	}
	return 0;
}

static int
fcgi_runloop_(void)
{
	int r;
	QUILTIMPLDATA *data;
	QUILTREQ *req;
		
	log_printf(LOG_DEBUG, "server is ready and waiting for FastCGI requests\n");
	data = (QUILTIMPLDATA *) calloc(1, sizeof(QUILTIMPLDATA));
	if(!data)
	{
		log_printf(LOG_CRIT, "failed to allocate memory for FastCGI requests\n");
		return -1;
	}
	while(1)
	{
		req = NULL;
		FCGX_InitRequest(&(data->req), quilt_fcgi_socket, 0);
		if(FCGX_Accept_r(&(data->req)) < 0)
		{
			log_printf(LOG_CRIT, "failed to accept FastCGI request\n");
			free(data);
			return -1;
		}
		if(fcgi_preprocess_(data))
		{
			r = -1;
		}
		else
		{
			req = quilt_request_create(&fcgi_impl, data);
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
				fcgi_fallback_error_(data, r);
			}
		}
		quilt_request_free(req);
		FCGX_Finish_r(&(data->req));
		free(data->qbuf);
		data->qbuf = NULL;
		data->query = NULL;
		if(r >= 500 && r <= 599)
		{
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

/* Obtain a filesystem path from a URI; returns -1 if an error
 * occurs, or 0 if there wasn't a valid path found
 */
static int
fcgi_sockpath_(URI *uri, char **ptr)
{
	char *p, *t;
	size_t l;
        
	*ptr = NULL;
	l = uri_path(uri, NULL, 0);
	if(l == (size_t) -1)
	{
		return -1;
	}
	if(l < 2)
	{               
		return 0;
	}
	p = (char *) malloc(l);
	if(!p)
	{
		return -1;
	}
	if(uri_path(uri, p, l) != l)
	{
		free(p);
		return -1;
	}
	if(p[0] != '/')
	{
		free(p);
		return 0;       
	}
	for(t = p; *t; t++)
	{
		if(*t != '/')
		{
			break;
		}
	}
	if(!*t)
	{
		/* The path was obviously empty */
		free(p);
		return 0;
	}
	*ptr = p;
	return 0;
}

static int
fcgi_hostport_(URI *uri, char **ptr)
{
	char *p;
	size_t l, hl, pl;
        
	*ptr = NULL;
	hl = uri_host(uri, NULL, 0);
	pl = uri_port(uri, NULL, 0);
	if(hl == (size_t) -1 || pl == (size_t) - 1)
	{
		return -1;
	}
	if(pl < 2)
	{
		return 0;
	}
	l = hl + pl + 1;
	p = (char *) malloc(l);
	if(!p)
	{
		return -1;
	}
	if(uri_host(uri, p, hl) != hl)
	{
		free(p);
		return -1;
	}
	p[hl - 1] = ':';
	if(uri_port(uri, &(p[hl]), pl) != pl)
	{
		free(p);
		return -1;
	}
	*ptr = p;
	return 0;
}

static int
fcgi_preprocess_(QUILTIMPLDATA *data)
{
	const char *qs, *s, *t;
	char *p;
	char cbuf[3];
	size_t n;

	data->headers_sent = 0;
	qs = FCGX_GetParam("QUERY_STRING", data->req.envp);
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
	return 0;
}

static
int fcgi_fallback_error_(QUILTIMPLDATA *data, int status)
{
	FCGX_FPrintF(data->req.out, "Status: %d Error\n"
				 "Content-type: text/html; charset=utf-8\n"
				 "Server: Quilt/" PACKAGE_VERSION "\n"
				 "\n", status);
	FCGX_FPrintF(data->req.out, "<!DOCTYPE html>\n"
				 "<html>\n"
				 "\t<head>\n"
				 "\t\t<meta charset=\"utf-8\">\n"
				 "\t\t<title>Error %d</title>\n"
				 "\t</head>\n"
				 "\t<body>\n"
				 "\t\t<h1>Error %d</h1>\n"
				 "\t\t<p>An error occurred while processing the request.</p>\n"
				 "\t</body>\n"
				 "</html>\n",				 
				 status, status);
	return 0;
}

/* QUILTIMPL methods */
static const char *
fcgi_getenv(QUILTREQ *request, const char *name)
{
	return FCGX_GetParam(name, request->data->req.envp);
}

static const char *
fcgi_getparam(QUILTREQ *request, const char *name)
{
	return FCGX_GetParam(name, request->data->query);
}

static int
fcgi_put(QUILTREQ *request, const unsigned char *str, size_t len)
{
	if(!request->data->headers_sent)
	{
		request->data->headers_sent = 1;
		FCGX_PutChar('\n', request->data->req.out);
	}
	return FCGX_PutStr((const char *) str, len, request->data->req.out);
}

static int
fcgi_vprintf(QUILTREQ *request, const char *format, va_list ap)
{
	if(!request->data->headers_sent)
	{
		request->data->headers_sent = 1;
		FCGX_PutChar('\n', request->data->req.out);
	}
	return FCGX_VFPrintF(request->data->req.out, format, ap);
}

static int
fcgi_header(QUILTREQ *request, const unsigned char *str, size_t len)
{
	if(request->data->headers_sent)
	{
		quilt_logf(LOG_WARNING, "cannot send headers; payload has already begun\n");
		return -1;
	}
	return FCGX_PutStr((const char *) str, len, request->data->req.out);
}

static int
fcgi_headerf(QUILTREQ *request, const char *format, va_list ap)
{
	if(request->data->headers_sent)
	{
		quilt_logf(LOG_WARNING, "cannot send headers; payload has already begun\n");
		return -1;
	}
	return FCGX_VFPrintF(request->data->req.out, format, ap);
}

static int
fcgi_begin(QUILTREQ *request)
{
	(void) request;
	
	return 0;
}

static int
fcgi_end(QUILTREQ *request)
{
	if(!request->data->headers_sent)
	{
		request->data->headers_sent = 1;
		FCGX_PutChar('\n', request->data->req.out);
	}
	return 0;
}

