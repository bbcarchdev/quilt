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

static int fcgi_sockpath_(URI *uri, char **ptr);
static int fcgi_hostport_(URI *uri, char **ptr);

static URI *quilt_fcgi_uri;
static int quilt_fcgi_socket = -1;

int
fcgi_init(void)
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

int
fcgi_runloop(void)
{
	int r;
	FCGX_Request *request;
	QUILTREQ *req;
		
	log_printf(LOG_DEBUG, "server is ready and waiting for FastCGI requests\n");
	request = (FCGX_Request *) calloc(1, sizeof(FCGX_Request));
	if(!request)
	{
		log_printf(LOG_CRIT, "failed to allocate memory for FastCGI requests\n");
		return -1;
	}
	while(1)
	{
		FCGX_InitRequest(request, quilt_fcgi_socket, 0);
		if(FCGX_Accept_r(request) < 0)
		{
			log_printf(LOG_CRIT, "failed to accept FastCGI request\n");
			return -1;
		}
		req = quilt_request_create_fcgi(request);
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
			if(!r)
			{
				r = req->status;
			}
		}
		if(r > 0 && r != 200)
		{
			quilt_error(request, r);
		}
		else if(r < 0)
		{
			quilt_error(request, 500);
		}
		quilt_request_free(req);
		FCGX_Finish_r(request);
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
