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

#include "p_libquilt.h"

static URI *quilt_base_uri;
static QUILTCB *quilt_engine_cb;
static QUILTCB *quilt_bulk_cb;

static int quilt_request_process_path_(QUILTREQ *req, const char *uri);
static const char *quilt_request_match_ext_(QUILTREQ *req);
static const char *quilt_request_match_mime_(QUILTREQ *req);

NEGOTIATE *quilt_types_;
NEGOTIATE *quilt_charsets_;

/* Internal: Initialise request handling */
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
	p = quilt_config_geta("quilt:base", NULL);
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

/* Internal: Once everything has been initialised and plug-ins loaded,
 * perform a sanity check
 */
int
quilt_request_sanity_(void)
{
	char *engine;

	engine = quilt_config_geta("quilt:engine", NULL);
	if(!engine)
	{
		quilt_logf(LOG_CRIT, "no engine was specified in the [quilt] section of the configuration file\n");
		return -1;
	}
	quilt_engine_cb = quilt_plugin_cb_find_name_(QCB_ENGINE, engine);
	if(!quilt_engine_cb)
	{
		quilt_logf(LOG_CRIT, "engine '%s' is unknown (has the relevant module been loaded?)\n", engine);
		free(engine);
		return -1;
	}
	quilt_bulk_cb = quilt_plugin_cb_find_name_(QCB_BULK, engine);	
	free(engine);
	return 0;
}

/* Internal: Create a request with a given request-URI; if URI is NULL, it
 * will be requested from the SAPI
 */
QUILTREQ *
quilt_request_create_uri_(QUILTIMPL *impl, QUILTIMPLDATA *data, const char *uri)
{
	QUILTREQ *p;
	const char *accept, *t;
	char date[32];
	struct tm now;
	librdf_world *world;

	p = (QUILTREQ *) calloc(1, sizeof(QUILTREQ));
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to allocate %u bytes for request structure\n", (unsigned) sizeof(QUILTREQ));
		return NULL;
	}
	p->impl = impl;
	p->data = data;
	p->received = time(NULL);
	p->host = impl->getenv(p, "REMOTE_ADDR");
	p->ident = impl->getenv(p, "REMOTE_IDENT");
	p->user = impl->getenv(p, "REMOTE_USER");
	p->method = impl->getenv(p, "REQUEST_METHOD");
	p->referer = impl->getenv(p, "HTTP_REFERER");
	p->ua = impl->getenv(p, "HTTP_USER_AGENT");
	p->baseuri = quilt_base_uri;
	p->base = uri_stralloc(quilt_base_uri);
	p->basegraph = quilt_node_create_uri(p->base);
	/* Log the request */
	gmtime_r(&(p->received), &now);
	strftime(date, sizeof(date), "%d/%b/%Y:%H:%M:%S +0000", &now);

	quilt_logf(LOG_DEBUG, "%s %s %s [%s] \"%s %s\" - - \"%s\" \"%s\"\n",
			   p->host, (p->ident ? p->ident : "-"), (p->user ? p->user : "-"),
			   date, p->method, uri, p->referer, p->ua);

	if(!uri)
	{
		uri = impl->getenv(p, "REQUEST_URI");
	}
   
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
		accept = impl->getenv(p, "HTTP_ACCEPT");
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
	p->canonext = quilt_request_match_mime_(p);
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
	quilt_logf(LOG_DEBUG, "negotiated type '%s' (extension '%s') from '%s'\n", p->type, p->canonext, accept);
	p->limit = DEFAULT_LIMIT;
	p->deflimit = DEFAULT_LIMIT;
	p->offset = 0;
	if((t = impl->getparam(p, "offset")) && t[0])
	{
		p->offset = strtol(t, NULL, 10);
	}
	if((t = impl->getparam(p, "limit")) && t[0])
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
	p->canonical = quilt_canon_create(NULL);
	if(!p)
	{
		p->status = 500;
		return p;
	}
	quilt_canon_set_base(p->canonical, p->base);
	quilt_canon_set_ext(p->canonical, p->canonext);
	quilt_canon_set_explicitext(p->canonical, p->ext);
	if(p->home)
	{
		quilt_canon_set_name(p->canonical, "index");
	}
	return p;	
}

/* SAPI: Invoked by the server to create a new request object */
QUILTREQ *
quilt_request_create(QUILTIMPL *impl, QUILTIMPLDATA *data)
{
	return quilt_request_create_uri_(impl, data, NULL);
}

int
quilt_request_bulk_item(QUILTBULK *bulk, const char *path)
{
	QUILTREQ *req;
	int r;

	req = quilt_request_create_uri_(bulk->impl, bulk->data, path);
	if(!req)
	{
		return -1;
	}
	if(req->status)
	{
		r = req->status;
		quilt_request_free(req);
		return r;
	}
	r = quilt_request_process(req);
	if(r < 0)
	{
		r = 500;
	}
	quilt_request_free(req);
	return r;
}

/* SAPI: Perform a bulk-generation request */
int
quilt_request_bulk(QUILTIMPL *impl, QUILTIMPLDATA *data, size_t offset, size_t limit)
{
	QUILTBULK bulk;

	if(!quilt_bulk_cb)
	{
		quilt_logf(LOG_CRIT, "the current engine does not support bulk-generation\n");
		return -1;
	}
	memset(&bulk, 0, sizeof(QUILTBULK));
	bulk.impl = impl;
	bulk.data = data;
	bulk.limit = limit;
	bulk.offset = offset;
	return quilt_plugin_invoke_bulk_(quilt_bulk_cb, &bulk);
}

/* Public: Obtain a request environment variable */
const char *
quilt_request_getenv(QUILTREQ *req, const char *name)
{
	return req->impl->getenv(req, name);
}

/* Public: Obtain a request query parameter */
const char *
quilt_request_getparam(QUILTREQ *req, const char *name)
{
	return req->impl->getparam(req, name);
}

/* Write a string to a request's output stream */
int
quilt_request_puts(QUILTREQ *req, const char *str)
{
	return req->impl->put(req, (const unsigned char *) str, strlen(str));
}

/* Write a byte sequence to a request's output stream */
int
quilt_request_put(QUILTREQ *req, const unsigned char *bytes, size_t len)
{
	return req->impl->put(req, bytes, len);
}

/* Perform formatted stream output */
int
quilt_request_printf(QUILTREQ *req, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	return req->impl->vprintf(req, format, ap);
}

int
quilt_request_vprintf(QUILTREQ *req, const char *format, va_list ap)
{
	return req->impl->vprintf(req, format, ap);
}

/* Write a header string to a request's output stream */
int
quilt_request_headers(QUILTREQ *req, const char *str)
{
	return req->impl->header(req, (const unsigned char *) str, strlen(str));
}

int
quilt_request_headerf(QUILTREQ *req, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	return req->impl->headerf(req, format, ap);
}

/* Return the base URI for all requests */
char *
quilt_request_base(void)
{
	return uri_stralloc(quilt_base_uri);
}

/* SAPI: Free the resources used by a request */
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

/* SAPI: Hand off the request to a processing engine; if needed, serialize the
 * model (i.e., if no error occurred and the engine didn't serialize itself).
 */
int
quilt_request_process(QUILTREQ *request)
{
	int r;

	r = request->impl->begin(request);
	if(r)
	{
		return r;
	}
	request->subject = uri_stralloc(request->uri);
	if(!request->subject)
	{
		quilt_logf(LOG_CRIT, "failed to unparse subject URI\n");
		request->impl->end(request);
		return 500;
	}
	quilt_logf(LOG_DEBUG, "query subject URI is <%s>\n", request->subject);	
	r = quilt_plugin_invoke_engine_(quilt_engine_cb, request);
	/* A zero return means the engine performed output itself; any other
	 * status indicates that output should be generated. If the status is 200,
	 * pass the request to the serializer.
	 */	
	if(r == 200)
	{
		r = quilt_request_serialize(request);
	}
	if(!request->headers_sent)
	{
		quilt_request_puts(request, "");
	}
	request->impl->end(request);
	return r;
}

/* SAPI: Serialize the model attached to a request according to the negotiated
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
	if(!request->status)
	{
		request->status = 200;
	}
	if(!request->statustitle)
	{
		request->statustitle = (request->status == 200 ? "OK" : "Error");
	}
	request->serialized = 1;
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
		quilt_logf(LOG_ERR, "malformed request-URI <%s>\n", uri);
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
static const char *
quilt_request_match_ext_(QUILTREQ *req)
{
	QUILTTYPE buf, *type;

	if(!req->ext)
	{
		return NULL;
	}
	type = quilt_plugin_serializer_match_ext(req->ext, &buf);
	if(!type)
	{
		return NULL;
	}
	return type->mimetype;
}

/* Return the preferred file extension, if any, for the negotiated type */
static const char *
quilt_request_match_mime_(QUILTREQ *req)
{
	QUILTTYPE buf, *type;
	
	type = quilt_plugin_serializer_match_mime(req->type, &buf);
	if(!type)
	{
		return NULL;
	}
	return type->extensions;
}
