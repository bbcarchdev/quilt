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

struct http_error_struct
{
	int code;
	const char *title;
	const char *description;
};

static struct http_error_struct errors[] =
{
	{ 200, "OK", "The request was completed successfully." },
	{ 400, "Bad request", "The request could not be understood by the server due to malformed syntax." },
	{ 401, "Unauthorized", "The request requires user authentication." },
	{ 402, "Payment required", "The request cannot be satisfied without inclusion of a payment token." },
	{ 403, "Forbidden", "The server understood the request, but is refusing to fulfill it." },
	{ 404, "Not found", "No resource matching the request could be found." },
	{ 405, "Method not allowed", "The request method is not supported by the resource." },
	{ 406, "Not acceptable", "The resource is not available in the requested serialisation." },
	{ 407, "Proxy authentication required", "The request requires proxy authentication."},
	{ 408, "Request timeout", "The client did not produce a request within the required time period." },
	{ 409, "Conflict", "The request could not be completed due to a conflict with the current state of the resource." },
	{ 410, "Gone", "The requested resource is no longer available." },
	{ 411, "Length required", "The request cannot be processed without a Content-Length." },
	{ 412, "Precondition failed", "A precondition associated with the request could not be satisfied." },
	{ 413, "Request entity too large", "The server is unable to process the request because the request entity is too large." },
	{ 414, "Request-URI too long", "The requested URI is longer than the server is able to process." },
	{ 415, "Unsupported media type", "The request cannot be processed because the entity of the request is not of a supported type." },
	{ 416, "Requested range not satisfiable", "The requested range of the request was not appropriate for the resource requested." },
	{ 417, "Expectation failed", "An expectation included in the request could not be satisfied." },
	{ 500, "Internal server error", "The server encountered an unexpected condition while processing the request." },
	{ 501, "Not implemented", "The server did not understand or does not support the HTTP method in the request." },
	{ 502, "Bad gateway", "An invalid response was received from an upstream server while processing the request." },
	{ 503, "Service unavailable", "The server is currently unable to service the request." },
	{ 504, "Gateway timeout", "The server did not receive a response from an upstream server in a timely fashion." },
	{ 505, "HTTP version not supported", "The server does not support the requested protocol version." },
	{ 0, NULL, NULL }
};

int
quilt_error(QUILTREQ *request, int code)
{
	size_t c;
	const char *sig, *title;
	char buf[64];

	sig = request->impl->getenv(request, "SERVER_SIGNATURE");
	for(c = 0; errors[c].code; c++)
	{
		if(errors[c].code == code)
		{
			break;
		}
	}
	if(errors[c].title)
	{
		title = errors[c].title;
	}
	else
	{
		sprintf(buf, "Error %d", code);
		title = buf;
	}
	request->impl->printf(request, "Status: %d %s\n"
				 "Content-type: text/html; charset=utf-8\n"
				 "Server: Quilt\n"
				 "\n", code, title);
	request->impl->printf(request, "<!DOCTYPE html>\n"
				 "<html>\n"
				 "\t<head>\n"
				 "\t\t<meta charset=\"utf-8\">\n"
				 "\t\t<title>%s</title>\n"
				 "\t</head>\n"
				 "\t<body>\n"
				 "\t\t<h1>%s</h1>\n",
				 title, title);
	if(errors[c].description)
	{
		request->impl->printf(request, "\t\t<p>%s</p>\n", errors[c].description);
	}
	else
	{
		request->impl->printf(request, "\t\t<p>No description of this error is available.</p>\n");
	}
	if(sig)
	{
		request->impl->printf(request, "\t\t<hr>\n"
					 "\t\t<p>%s</p>\n", sig);
	}
	request->impl->printf(request, "\t</body>\n",
				 "</html>\n");
	return 0;
}

				 
