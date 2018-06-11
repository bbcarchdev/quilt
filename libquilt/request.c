/* Quilt: A Linked Open Data server
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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
	quilt_canon_set_user_path(p->canonical, uri);
	quilt_canon_set_user_query(p->canonical, impl->getenv(p, "QUERY_STRING"));
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

/* Public: Obtain a request query parameter as an integer */
long
quilt_request_getparam_int(QUILTREQ *req, const char *name)
{
	const char *t;
	long l;
	char *endp;

	t = req->impl->getparam(req, name);
	if(!t)
	{
		return 0;
	}
	endp = NULL;
	l = strtol(t, &endp, 10);
	if(endp && endp[0])
	{
		return 0;
	}
	return l;
}

/* Public: Obtain a request query parameter with multiple values */
const char *const *
quilt_request_getparam_multi(QUILTREQ *req, const char *name)
{
	return req->impl->getparam_multi(req, name);
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
	if(req->impl)
	{
		req->impl->end(req);
	}
	if(req->uri)
	{
		uri_destroy(req->uri);
	}
	if(req->base)
	{
		free(req->base);
	}
	if(req->basegraph)
	{
		librdf_free_node(req->basegraph);
	}
	if(req->canonical)
	{
		quilt_canon_destroy(req->canonical);
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
	free(request->subject);
	request->subject = NULL;
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
	t = strchr(buf, '#');
	if(t)
	{
		*t = 0;
	}
	req->path = buf;
	t = strchr(buf, '?');
	if(t)
	{
		*t = 0;
		t++;
		if(*t)
		{
			req->query = t;
		}
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
	quilt_logf(LOG_DEBUG, "Path: %s\n", req->path);
	quilt_logf(LOG_DEBUG, "Query: %s\n", req->query);
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

/* Property accessors */

QUILTIMPLDATA *
quilt_request_impldata(QUILTREQ *req)
{
	return req->data;
}

int
quilt_request_serialized(QUILTREQ *req)
{
	return req->serialized;
}

URI *
quilt_request_uri(QUILTREQ *req)
{
	return req->uri;
}

URI *
quilt_request_baseuri(QUILTREQ *req)
{
	return req->baseuri;
}

const char *
quilt_request_baseuristr(QUILTREQ *req)
{
	return req->base;
}

const char *
quilt_request_host(QUILTREQ *req)
{
	return req->host;
}

const char *
quilt_request_ident(QUILTREQ *req)
{
	return req->ident;
}

const char *
quilt_request_user(QUILTREQ *req)
{
	return req->user;
}

const char *
quilt_request_method(QUILTREQ *req)
{
	return req->method;
}

const char *
quilt_request_referer(QUILTREQ *req)
{
	return req->referer;
}

const char *
quilt_request_ua(QUILTREQ *req)
{
	return req->ua;
}

const char *
quilt_request_path(QUILTREQ *req)
{
	return req->path;
}

const char *
quilt_request_ext(QUILTREQ *req)
{
	return req->ext;
}

time_t
quilt_request_received(QUILTREQ *req)
{
	return req->received;
}

int
quilt_request_status(QUILTREQ *req)
{
	return req->status;
}

const char *
quilt_request_statustitle(QUILTREQ *req)
{
	return req->statustitle;
}

const char *
quilt_request_statusdesc(QUILTREQ *req)
{
	return req->errordesc;
}

librdf_node *
quilt_request_basegraph(QUILTREQ *req)
{
	return req->basegraph;
}

librdf_storage *
quilt_request_storage(QUILTREQ *req)
{
	return req->storage;
}

librdf_model *
quilt_request_model(QUILTREQ *req)
{
	return req->model;
}

const char *
quilt_request_subject(QUILTREQ *req)
{
	return req->subject;
}

int
quilt_request_set_subject_uristr(QUILTREQ *req, const char *uristr)
{
	char *p;

	p = strdup(uristr);
	if(!p)
	{
		return -1;
	}
	free(req->subject);
	req->subject = p;
	return 0;
}

int
quilt_request_home(QUILTREQ *req)
{
	return req->home;
}

int
quilt_request_index(QUILTREQ *req)
{
	return req->index;
}

const char *
quilt_request_indextitle(QUILTREQ *req)
{
	return req->indextitle;
}

int
quilt_request_limit(QUILTREQ *req)
{
	return req->limit;
}

int
quilt_request_deflimit(QUILTREQ *req)
{
	return req->deflimit;
}

int
quilt_request_offset(QUILTREQ *req)
{
	return req->offset;
}

const char *
quilt_request_type(QUILTREQ *req)
{
	return req->type;
}

const char *
quilt_request_typeext(QUILTREQ *req)
{
	return req->canonext;
}

QUILTCANON *
quilt_request_canonical(QUILTREQ *req)
{
	return req->canonical;
}

char *
quilt_request_query(QUILTREQ *req)
{
	return req->query;
}

int
quilt_request_set_graph_uristr(QUILTREQ *req, const char *graph)
{
	URI *uri;
	char *uristr;
	librdf_node *node;

	if(!graph)
	{
		return -1;
	}
	uri = uri_create_str(graph, req->baseuri);
	if(!uri)
	{
		return -1;
	}
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		uri_destroy(uri);
		return -1;
	}
	node = librdf_new_node_from_uri_string(quilt_librdf_world(), (const unsigned char *) uristr);
	if(!node)
	{
		free(uristr);
		uri_destroy(uri);
		return -1;
	}
	if(req->graph)
	{
		librdf_free_node(req->graph);
	}
	if(req->graphuristr)
	{
		free(req->graphuristr);
	}
	if(req->graphuri)
	{
		uri_destroy(req->graphuri);
	}
	req->graphuri = uri;
	req->graphuristr = uristr;
	req->graph = node;
	return 0;
}

librdf_node *
quilt_request_graph(QUILTREQ *req)
{
	char *concrete;
	int r;

	if(!req->graph)
	{
		concrete = quilt_canon_str(req->canonical, ((req->ext != NULL) ? QCO_REQUEST : QCO_CONCRETE));
		r = quilt_request_set_graph_uristr(req, concrete);
		free(concrete);
		if(r)
		{
			return NULL;
		}
	}
	return req->graph;
}

const char *
quilt_request_graph_uristr(QUILTREQ *req)
{
	if(!quilt_request_graph(req))
	{
		return NULL;
	}
	return req->graphuristr;
}

/* Path consumption:
 *
 * quilt_request_rewind() resets the pointer to the start of the path
 * buffer
 *
 * quilt_request_peek() returns the next component of the path, after
 * urldecoding, but keeps the pointer where it is
 *
 * quilt_request_consume() returns the next component of the path, after
 * urldecoding, but advances the pointer to the component following it,
 * which would be returned by subsequent calls to quilt_request_peek()
 * or quilt_request_consume()
 *
 * Both peek and consume will return NULL if the end of the path is
 * reached, and will continue to return NULL until quilt_request_rewind()
 * is called.
 */

int
quilt_request_rewind(QUILTREQ *req)
{
	/* If the structure has not been initialised yet, allocate our buffers
	 */
	if(!req->consume.buf)
	{
		req->consume.buflen = strlen(req->path) + 8;
		req->consume.buf = (char *) calloc(1, req->consume.buflen);
		req->consume.labuf = (char *) calloc(1, req->consume.buflen);
	}

	/* Now set the pointers to the right places: initially
	 * req->consume.cur will be NULL, and req->consume.next
	 * will point to the start of the first path component,
	 * or to the end of the string, whichever comes first.
	 */
	req->consume.cur = NULL;
	/* Skip any leading slashes */
	for(req->consume.next = req->path; req->consume.next[0] && req->consume.next[0] == '/'; req->consume.next++);
	return 0;
}

/* Peek at the next path component (i.e., where req->consume.next points) */
const char *
quilt_request_peek(QUILTREQ *req)
{
	unsigned long chr, n;
	const char *endp;
	int q;
	size_t c;

	if(!req->consume.buf)
	{
		if(quilt_request_rewind(req))
		{
			return NULL;
		}
	}
	if(!req->consume.next[0])
	{
		/* End of path */
		return NULL;
	}
	/* If the look-ahead buffer, req->consume.labuf, is empty, populate
	 * it with the urldecoded version of the next path component
	 */
	if(!req->consume.labuf[0])
	{
		c = 0;
		q = 0;
		for(endp = req->consume.next; *endp && *endp != '/'; endp++)
		{
			if(q)
			{
				if(isxdigit(*endp))
				{
					/* XXX this will break on EBCDIC or other non-ASCII systems */
					if(*endp >= '0' && *endp <= '9')
					{
						n = *endp - '0';
					}
					else if(*endp >= 'A' && *endp <= 'F')
					{
						n = *endp - 'A' + 10;
					}
					else if(*endp >= 'a' && *endp <= 'f')
					{
						n = *endp - 'a' + 10;
					}
					chr = (chr << 4) | n;
					continue;
				}
				else
				{
					req->consume.labuf[c] = chr;
					q = 0;
					c++;
				}
			}
			if(!q)
			{
				if(*endp == '%')
				{
					q = 1;
					chr = 0;
					continue;
				}
				req->consume.labuf[c] = *endp;
				c++;
			}
		}	   
		if(q)
		{
			req->consume.labuf[c] = *endp;
			c++;
		}
		req->consume.labuf[c] = 0;
	}
	return req->consume.labuf;
}

/* Consume the next path component, advancing the pointer */
const char *
quilt_request_consume(QUILTREQ *req)
{
	/* Invoking quilt_request_peek() ensures that the consume
	 * structure is properly populated, and the look-ahead buffer
	 * and 'next' pointer are both pointing to where they're meant
	 * to be.
	 */
	if(!quilt_request_peek(req))
	{
		/* Nothing left in the path */
		return NULL;
	}
	/* After quilt_request_peek(), req->consume.next and req->consume.labuf
	 * contain everything we need to return, so copy them before advancing to
	 * the next component.
	 */
	req->consume.cur = req->consume.next;
	strcpy(req->consume.buf, req->consume.labuf);
	/* Reset the look-ahead buffer so that the next peek or consume operation
	 * will repopulate it.
	 */
	req->consume.labuf[0] = 0;
	/* Update req->consume.next to point to the start of the
	 * NEXT component after this one
	 */
	for(; req->consume.next[0]; req->consume.next++)
	{
		if(req->consume.next[0] == '/')
		{
			break;
		}
	}
	if(req->consume.next[0])
	{
		/* Skip any slashes */
		for(; req->consume.next[0] == '/'; req->consume.next++);
	}
	/* At this point:
	 *   req->consume.buf contains what was in the look-ahead buffer
	 *   req->consume.cur points to the start of that component in the path
	 *   req->consume.labuf is empty
	 *   req->consume.next points to the start of the next component, or the
	 *     end of the path string, whichever arrived first.
	 */
	return req->consume.buf;
}
