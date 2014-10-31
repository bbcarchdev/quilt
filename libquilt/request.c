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

#include "p_libquilt.h"

static URI *quilt_base_uri;
static QUILTCB *quilt_engine_cb;

struct typemap_struct quilt_typemap[] = 
{
	{ "txt", "text/plain", "Plain text", 0 },
	{ "ttl", "text/turtle", "Turtle", 1 },
	{ "n3", "text/rdf+n3", "N3", 0 },
	{ "gv", "text/x-graphviz", "Graphviz", 0 },
	{ "nq", "text/x-nquads", "NQuads", 1 },
	{ "html", "text/html", "HTML", 1 },
	{ "xml", "text/xml", "XML", 0 },
	{ "nt", "application/n-triples", "NTriples", 1 },
	{ "rdf", "application/rdf+xml", "RDF/XML", 1 },
	{ "rss", "application/rss+xml", "RSS", 0 },
	{ "atom", "application/atom+xml", "Atom", 0 },
	{ "xhtml", "application/xhtml+xml", "XHTML", 0 },
	{ "json", "application/json", "RDF/JSON", 1 },
	{ NULL, NULL, NULL, 0 }
};

static int quilt_request_process_path_(QUILTREQ *req, const char *uri);
static const char *quilt_request_match_ext_(QUILTREQ *req);
static int quilt_request_parse_params_(QUILTREQ *request);

NEGOTIATE *quilt_types_;
NEGOTIATE *quilt_charsets_;

/* Initialise request handling */
int
quilt_request_init_(void)
{
	char *p;

	quilt_types_ = neg_create();
	quilt_charsets_ = neg_create();
	if(!quilt_types_ || !quilt_charsets_)
	{
		quilt_logf(LOG_CRIT, "failed to create new negotiation objects\n");
		return -1;
	}
	neg_add(quilt_charsets_, "utf-8", 1);
	p = config_geta("quilt:base", NULL);
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to determine base URI from configuration\n");
		return -1;
	}
	quilt_base_uri = uri_create_str(p, NULL);
	if(!quilt_base_uri)
	{
		quilt_logf(LOG_CRIT, "failed to parse <%s> as a URI\n", p);
		free(p);
		return -1;
	}
	quilt_logf(LOG_DEBUG, "base URI is <%s>\n", p);
	free(p);
	return 0;
}

/* Once everything has been initialised and plug-ins loaded, perform a
 * sanity check
 */
int
quilt_request_sanity_(void)
{
	const char *engine;
	
	engine = config_getptr_unlocked("quilt:engine", NULL);
	if(!engine)
	{
		quilt_logf(LOG_CRIT, "no engine was specified in the [quilt] section of the configuration file\n");
		return -1;
	}
	quilt_engine_cb = quilt_plugin_cb_find_name_(QCB_ENGINE, engine);
	if(!quilt_engine_cb)
	{
		quilt_logf(LOG_CRIT, "engine '%s' is unknown (has the relevant module been loaded?)\n", engine);
		return -1;
	}
	return 0;
}

QUILTREQ *
quilt_request_create_fcgi(FCGX_Request *request)
{	
	QUILTREQ *p;
	const char *accept, *uri, *t;
	char date[32];
	struct tm now;
	librdf_world *world;
	
	p = (QUILTREQ *) calloc(1, sizeof(QUILTREQ));
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to allocate %u bytes for request structure\n", (unsigned) sizeof(QUILTREQ));
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
	p->baseuri = quilt_base_uri;
	p->base = uri_stralloc(quilt_base_uri);
	p->basegraph = quilt_node_create_uri(p->base);
	/* Log the request */
	gmtime_r(&(p->received), &now);
	strftime(date, sizeof(date), "%d/%b/%Y:%H:%M:%S +0000", &now);

	uri = FCGX_GetParam("REQUEST_URI", request->envp);

	quilt_logf(LOG_DEBUG, "%s %s %s [%s] \"%s %s\" - - \"%s\" \"%s\"\n",
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
		quilt_logf(LOG_ERR, "failed to parse <%s> into a URI\n", p->path);
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
	p->type = neg_negotiate_type(quilt_types_, accept);	
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
		quilt_logf(LOG_CRIT, "failed to create new RDF storage\n");
		p->status = 500;
		return p;
	}
	p->model = librdf_new_model(world, p->storage, NULL);
	if(!p->model)
	{
		quilt_logf(LOG_CRIT, "failed to create new RDF model\n");
		p->status = 500;
		return p;
	}
	quilt_logf(LOG_DEBUG, "negotiated type '%s' from '%s'\n", p->type, accept);
	quilt_request_parse_params_(p);
	p->limit = DEFAULT_LIMIT;
	p->offset = 0;
	if((t = FCGX_GetParam("offset", p->query)))
	{
		p->offset = strtol(t, NULL, 10);
	}
	if((t = FCGX_GetParam("limit", p->query)))
	{
		p->limit = strtol(t, NULL, 10);
	}
	if(p->offset < 0)
	{
		p->offset = 0;
	}
	if(p->limit < 1)
	{
		p->limit = 1;
	}
	if(p->limit > MAX_LIMIT)
	{
		p->limit = MAX_LIMIT;
	}
	return p;
}

static int
quilt_request_parse_params_(QUILTREQ *request)
{
	const char *qs, *s, *t;
	char *p;
	char cbuf[3];
	size_t n;

	qs = FCGX_GetParam("QUERY_STRING", request->fcgi->envp);
	if(!qs)
	{
		request->qbuf = strdup("");
		request->query = (char **) calloc(1, sizeof(char *));
		return 0;
	}
	request->qbuf = (char *) calloc(1, strlen(qs) + 1);
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
	request->query = (char **) calloc(n + 1, sizeof(char *));
	p = request->qbuf;
	s = qs;
	n = 0;
	while(s)
	{
		request->query[n] = p;
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

char *
quilt_request_base(void)
{
	return uri_stralloc(quilt_base_uri);
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
	free(req->qbuf);
	free(req->query);
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

	request->subject = uri_stralloc(request->uri);
	if(!request->subject)
	{
		quilt_logf(LOG_CRIT, "failed to unparse subject URI\n");
		return 500;
	}
	quilt_logf(LOG_DEBUG, "query subject URI is <%s>\n", request->subject);
	r = quilt_plugin_invoke_engine_(quilt_engine_cb, request);
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
	QUILTCB *cb;

	cb = quilt_plugin_cb_find_mime_(QCB_SERIALIZE, request->type);
	if(!cb)
	{
		quilt_logf(LOG_ERR, "failed to serialise model as %s\n", request->type);
		return 406;
	}		
	return quilt_plugin_invoke_serialize_(cb, request);
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
		quilt_logf(LOG_CRIT, "failed to duplicate request-URI: %s\n", strerror(errno));
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
	if(req->path[0] == '/' && req->path[1] == 0)
	{
		req->home = 1;
		req->index = 1;
	}
	return 0;
}

/* Match the extension within a request to a media type in the typemap
 * list.
 */

/* XXX examine the extensions in registered serializer callbacks */
static const char *
quilt_request_match_ext_(QUILTREQ *req)
{
	size_t c;
	
	for(c = 0; quilt_typemap[c].ext; c++)
	{
		if(!strcmp(req->ext, quilt_typemap[c].ext))
		{
			return quilt_typemap[c].type;
		}
	}
	return NULL;
}
