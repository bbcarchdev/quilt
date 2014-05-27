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

struct typemap_struct
{
	const char *ext;
	const char *type;
};

static URI *quilt_base_uri;
static struct typemap_struct typemap[] = 
{
	{ "txt", "text/plain" },
	{ "ttl", "text/turtle" },
	{ "n3", "text/rdf+n3" },
	{ "gv", "text/x-graphviz" },
	{ "nq", "text/x-nquads" },
	{ "html", "text/html" },
	{ "xml", "text/xml" },
	{ "nt", "application/n-triples" },
	{ "rdf", "application/rdf+xml" },
	{ "rss", "application/rss+xml" },
	{ "atom", "application/atom+xml" },
	{ "xhtml", "application/xhtml+xml" },
	{ "json", "application/json" },
	{ NULL, NULL }
};

static int quilt_request_process_path_(QUILTREQ *req, const char *uri);
static const char *quilt_request_match_ext_(QUILTREQ *req);

NEGOTIATE *quilt_types;
NEGOTIATE *quilt_charsets;

int
quilt_request_init(void)
{
	char *p;
	
	p = config_geta("quilt:base", NULL);
	if(!p)
	{
		log_printf(LOG_CRIT, "failed to determine base URI from configuration\n");
		return -1;
	}
	quilt_base_uri = uri_create_str(p, NULL);
	if(!quilt_base_uri)
	{
		log_printf(LOG_CRIT, "failed to parse <%s> as a URI\n", p);
		free(p);
		return -1;
	}
	log_printf(LOG_DEBUG, "base URI is <%s>\n", p);
	free(p);
	return 0;
}

QUILTREQ *
quilt_request_create_fcgi(FCGX_Request *request)
{	
	QUILTREQ *p;
	const char *accept, *uri;
	char date[32];
	struct tm now;
	librdf_world *world;
	
	p = (QUILTREQ *) calloc(1, sizeof(QUILTREQ));
	if(!p)
	{
		log_printf(LOG_CRIT, "failed to allocate %u bytes for request structure\n", (unsigned) sizeof(QUILTREQ));
		return NULL;
	}
	p->fcgi = request;
	p->received = time(NULL);
	p->host = FCGX_GetParam("REMOTE_ADDR", request->envp);
	p->ident = FCGX_GetParam("REMOTE_IDENT", request->envp);
	p->user = FCGX_GetParam("REMOTE_USER", request->envp);
	p->method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	p->referer = FCGX_GetParam("HTTP_REFERER", request->envp);
	p->ua = FCGX_GetParam("HTTP_USER_AGENT", request->envp);

	/* Log the request */
	gmtime_r(&(p->received), &now);
	strftime(date, sizeof(date), "%d/%b/%Y:%H:%M:%S +0000", &now);

	uri = FCGX_GetParam("REQUEST_URI", request->envp);

	log_printf(LOG_DEBUG, "%s %s %s [%s] \"%s %s\" - - \"%s\" \"%s\"\n",
			   p->host, (p->ident ? p->ident : "-"), (p->user ? p->user : "-"),
			   date, p->method, uri, p->referer, p->ua);
   
	if(quilt_request_process_path_(p, uri))
	{
		p->status = 400;
		return p;
	}
	
	p->uri = uri_create_str(p->path, quilt_base_uri);
	if(!p->uri)
	{
		log_printf(LOG_ERR, "failed to parse <%s> into a URI\n", p->path);
		p->status = 400;
		return p;
	}
	if(p->ext)
	{
		accept = quilt_request_match_ext_(p);
		if(!accept)
		{
			p->status = 406;
			return p;
		}
	}
	else
	{
		accept = FCGX_GetParam("HTTP_ACCEPT", request->envp);
		if(!accept)
		{
			accept = "*/*";
		}
	}
	p->type = neg_negotiate_type(quilt_types, accept);	
	if(!p->type)
	{
		p->status = 406;
		return p;
	}
	world = quilt_librdf_world();
	if(!world)
	{
		p->status = 500;
		return p;
	}
	p->storage = librdf_new_storage(world, "hashes", NULL, "hash-type='memory',contexts='yes'");
	if(!p->storage)
	{
		log_printf(LOG_CRIT, "failed to create new RDF storage\n");
		p->status = 500;
		return p;
	}
	p->model = librdf_new_model(world, p->storage, NULL);
	if(!p->model)
	{
		log_printf(LOG_CRIT, "failed to create new RDF model\n");
		p->status = 500;
		return p;
	}
	log_printf(LOG_DEBUG, "negotiated type '%s' from '%s'\n", p->type, accept);
	return p;
}

int
quilt_request_free(QUILTREQ *req)
{
	if(req->uri)
	{
		uri_destroy(req->uri);
	}
	if(req->model)
	{
		librdf_free_model(req->model);
	}
	if(req->storage)
	{
		librdf_free_storage(req->storage);
	}
	free(req->path);
	free(req);
	return 0;
}

/* Hand off the request to a processing engine; if needed, serialize the
 * model (i.e., if no error occurred and the engine didn't serialize itself).
 */
int
quilt_request_process(QUILTREQ *request)
{
	int r;

	r = quilt_engine_resourcegraph_process(request);
	if(r)
	{
		return r;
	}
	return quilt_request_serialize(request);
}

/* Serialize the model attached to a request according to the negotiated
 * response media type.
 */
int
quilt_request_serialize(QUILTREQ *request)
{
	char *buf;
	
	buf = quilt_model_serialize(request->model, request->type);
	if(!buf)
	{
		log_printf(LOG_ERR, "failed to serialise model as %s\n", request->type);
		return 406;
	}	
	FCGX_FPrintF(request->fcgi->out, "Status: 200 OK\n"
				 "Content-type: %s\n"
				 "Vary: Accept\n"
				 "Server: Quilt\n"
				 "\n", request->type);
	FCGX_PutStr(buf, strlen(buf), request->fcgi->out);
	free(buf);
	return 200;
}

/* Process a REQUEST_URI from the environment and populate the
 * relevant portions of the request data
 */
static int
quilt_request_process_path_(QUILTREQ *req, const char *uri)
{
	char *buf, *t;

	if(!uri || uri[0] != '/')
	{
		/* Bad request */
		return -1;
	}
	buf = strdup(uri);
	if(!buf)
	{
		log_printf(LOG_CRIT, "failed to duplicate request-URI: %s\n", strerror(errno));
		return -1;
	}
	req->path = buf;
	t = strchr(buf, '?');
	if(t)
	{
		*t = 0;
	}
	t = strchr(buf, '.');
	if(t)
	{
		*t = 0;
		t++;
		if(*t)
		{
			req->ext = t;
		}
	}
	/* Translate '/index' to '/' */
	if(!strcmp(req->path, "/index"))
	{
		req->path[1] = 0;
	}
	return 0;
}

/* Match the extension within a request to a media type in the typemap
 * list.
 */

static const char *
quilt_request_match_ext_(QUILTREQ *req)
{
	size_t c;
	
	for(c = 0; typemap[c].ext; c++)
	{
		if(!strcmp(req->ext, typemap[c].ext))
		{
			return typemap[c].type;
		}
	}
	return NULL;
}
