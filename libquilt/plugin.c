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

/* Linked list of registered callbacks */
static QUILTCB *cb_first, *cb_last;
/* The current module handle */
static void *current;

static int quilt_plugin_load_cb_(const char *key, const char *value, void *data);
static QUILTMIME *mime_create(const QUILTTYPE *type);
static void mime_destroy(QUILTMIME *mime);
static void quilt_copy_quilttype_(QUILTTYPE *dest, QUILTCB *src);

/* Private: initialise plug-ins
 * Note that plug-in registration functions will be invoked BEFORE
 * quilt_plugin_init_() (for example by the built-in RDF serializer to
 * register itself.
 */
int
quilt_plugin_init_(void)
{
	quilt_config_get_all("quilt", "module", quilt_plugin_load_cb_, NULL);
	return 0;
}

/* Internal: configuration enumerator for loading plug-ins */
static int
quilt_plugin_load_cb_(const char *key, const char *value, void *data)
{
	(void) key;
	(void) data;

	quilt_plugin_load_(value);
	return 0;
}

/* Internal: load a plug-in */
int 
quilt_plugin_load_(const char *pathname)
{
	void *handle;
	quilt_plugin_init_fn fn;
	char *fnbuf;
	size_t len;

	quilt_logf(LOG_DEBUG, "loading plug-in %s\n", pathname);
	if(strchr(pathname, '/'))
	{
		fnbuf = NULL;
	}
	else
	{
		len = strlen(pathname) + strlen(PLUGINDIR) + 1;
		fnbuf = (char *) malloc(len);
		if(!fnbuf)
		{
			quilt_logf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) len);
			return -1;
		}
		strcpy(fnbuf, PLUGINDIR);
		strcat(fnbuf, pathname);
		pathname = fnbuf;
	}
	handle = dlopen(pathname, RTLD_NOW);
	if(!handle)
	{
		quilt_logf(LOG_ERR, "failed to load %s: %s\n", pathname, dlerror());
		free(fnbuf);
		return -1;
	}
	fn = (quilt_plugin_init_fn) dlsym(handle, "quilt_plugin_init");
	if(!fn)
	{
		quilt_logf(LOG_ERR, "%s is not a Quilt plug-in\n", pathname);
		dlclose(handle);
		free(fnbuf);
		errno = EINVAL;
		return -1;
	}
	quilt_logf(LOG_DEBUG, "invoking plug-in initialisation function for %s\n", pathname);
	current = handle;
	if(fn())
	{
		quilt_logf(LOG_ERR, "initialisation of plug-in %s failed\n", pathname);
		current = NULL;
		quilt_plugin_unload_(handle);           
		free(fnbuf);
		return -1;
	}
	quilt_logf(LOG_INFO, "loaded plug-in %s\n", pathname);
	free(fnbuf);
	current = NULL;
	return 0;
}

/* Internal: unload a plug-in, de-registering any callbacks */
int
quilt_plugin_unload_(void *handle)
{
	(void) handle;

	return 0;
}

/* Internal: add a new callback to the list */
QUILTCB *
quilt_plugin_cb_add_(void *handle, const char *name)
{
	QUILTCB *p;

	p = (QUILTCB *) calloc(1, sizeof(QUILTCB));
	if(!p)
	{
		return NULL;
	}
	if(name)
	{
		p->name = strdup(name);
		if(!p->name)
		{
			free(p);
			return NULL;
		}
	}
	p->handle = handle;
	if(cb_first)
	{
		p->prev = cb_last;
		cb_last->next = p;
	}
	else
	{
		cb_first = p;
	}
	cb_last = p;
	return p;
}

/* Internal: locate a callback of a particular type for the given MIME type */
QUILTCB *
quilt_plugin_cb_find_mime_(QUILTCBTYPE type, const char *mimetype)
{
	QUILTCB *cur;

	if(!mimetype)
	{
		return NULL;
	}
	for(cur = cb_first; cur; cur = cur->next)
	{
		if(cur->type != type || !cur->mime)
		{
			continue;
		}
		if(!strcasecmp(cur->mime->mimetype, mimetype))
		{
			return cur;
		}
	}
	return NULL;
}

/* Internal: locate a callback of a particular type with a given name */
QUILTCB *
quilt_plugin_cb_find_name_(QUILTCBTYPE type, const char *name)
{
	QUILTCB *cur;

	for(cur = cb_first; cur; cur = cur->next)
	{
		if(cur->type != type || !cur->name)
		{
			continue;
		}
		if(!strcasecmp(cur->name, name))
		{
			return cur;
		}
	}
	return NULL;
}

int
quilt_plugin_register_serializer(const QUILTTYPE *type, quilt_serialize_fn fn)
{
	QUILTCB *cb;
	QUILTMIME *mime;

	mime = mime_create(type);
	if(!mime)
	{
		return -1;
	}
	cb = quilt_plugin_cb_find_mime_(QCB_SERIALIZE, type->mimetype);
	if(cb)
	{
		/* Replace the existing entry */
		free(cb->name);
		cb->name = NULL;
		mime_destroy(cb->mime);
		cb->mime = mime;
		cb->handle = current;
		cb->cb.serialize = fn;
		neg_add(quilt_types_, mime->mimetype, mime->qs);
		quilt_logf(LOG_DEBUG, "registered replacement serializer for %s (%f)\n", type->mimetype, type->qs);
		return 0;
	}
	cb = quilt_plugin_cb_add_(current, NULL);
	if(!cb)
	{
		mime_destroy(mime);
		return -1;
	}
	cb->mime = mime;
	cb->cb.serialize = fn;
	cb->type = QCB_SERIALIZE;
	neg_add(quilt_types_, mime->mimetype, mime->qs);
	quilt_logf(LOG_DEBUG, "registered serializer for %s (%f)\n", type->mimetype, type->qs);
	return 0;
}

int
quilt_plugin_register_engine(const char *name, quilt_engine_fn fn)
{
	QUILTCB *cb;
	char *namebuf;

	cb = quilt_plugin_cb_find_name_(QCB_ENGINE, name);
	if(cb)
	{
		quilt_logf(LOG_ERR, "engine '%s' has already been registered\n", name);
		return -1;
	}
	namebuf = strdup(name);
	if(!namebuf)
	{
		return -1;
	}
	cb = quilt_plugin_cb_add_(current, NULL);
	if(!cb)
	{
		free(namebuf);
		return -1;
	}
	cb->name = namebuf;
	cb->cb.serialize = fn;
	cb->type = QCB_ENGINE;
	quilt_logf(LOG_DEBUG, "registered engine '%s'\n", name);
	return 0;
}

int
quilt_plugin_register_bulk(const char *name, quilt_bulk_fn fn)
{
	QUILTCB *cb;
	char *namebuf;

	cb = quilt_plugin_cb_find_name_(QCB_BULK, name);
	if(cb)
	{
		quilt_logf(LOG_ERR, "bulk-generation engine '%s' has already been registered\n", name);
		return -1;
	}
	namebuf = strdup(name);
	if(!namebuf)
	{
		return -1;
	}
	cb = quilt_plugin_cb_add_(current, NULL);
	if(!cb)
	{
		free(namebuf);
		return -1;
	}
	cb->name = namebuf;
	cb->cb.bulk = fn;
	cb->type = QCB_BULK;
	quilt_logf(LOG_DEBUG, "registered bulk-generation engine '%s'\n", name);
	return 0;
}

int
quilt_plugin_invoke_engine_(QUILTCB *cb, QUILTREQ *req)
{
	int r;
	void *old;

	if(cb->type != QCB_ENGINE)
	{
		quilt_logf(LOG_CRIT, "internal error: attempt to invoke a %d callback as a query engine\n", cb->type);
		errno = EINVAL;
		return -1;
	}
	old = current;
	current = cb->handle;
	r = cb->cb.engine(req);
	current = old;
	return r;
}

int
quilt_plugin_invoke_bulk_(QUILTCB *cb, QUILTBULK *bulk)
{
	int r;
	void *old;

	if(cb->type != QCB_BULK)
	{
		quilt_logf(LOG_CRIT, "internal error: attempt to invoke a %d callback as a bulk generator\n", cb->type);
		errno = EINVAL;
		return -1;
	}
	old = current;
	current = cb->handle;
	r = cb->cb.bulk(bulk, bulk->offset, bulk->limit);
	current = old;
	return r;
}

int
quilt_plugin_invoke_serialize_(QUILTCB *cb, QUILTREQ *req)
{
	int r;
	void *old;

	if(cb->type != QCB_SERIALIZE)
	{
		quilt_logf(LOG_CRIT, "internal error: attempt to invoke a %d callback as a serializer\n", cb->type);
		errno = EINVAL;
		return -1;
	}
	old = current;
	current = cb->handle;
	quilt_logf(LOG_DEBUG, "invoking the callback for '%s'\n", cb->mime->mimetype);
	r = cb->cb.serialize(req);
	current = old;
	return r;
}

QUILTTYPE *
quilt_plugin_serializer_match_ext(const char *ext, QUILTTYPE *dest)
{
	QUILTCB *cur;
	size_t c;
	for(cur = cb_first; cur; cur = cur->next)
	{
		if(cur->type != QCB_SERIALIZE)
		{
			continue;
		}
		if(!cur->mime->extensions)
		{
			continue;
		}
		for(c = 0; cur->mime->extensions[c]; c++)
		{
			if(!strcasecmp(ext, cur->mime->extensions[c]))
			{
				quilt_copy_quilttype_(dest, cur);
				return dest;
			}
		}
	}
	return NULL;
}

QUILTTYPE *
quilt_plugin_serializer_match_mime(const char *mime, QUILTTYPE *dest)
{
	QUILTCB *cur;
	
	for(cur = cb_first; cur; cur = cur->next)
	{
		if(cur->type != QCB_SERIALIZE)
		{
			continue;
		}
		if(!strcasecmp(mime, cur->mime->mimetype))
		{
			quilt_copy_quilttype_(dest, cur);
			return dest;
		}
	}
	return NULL;
}


/* Obtain a QUILTTYPE for a plugin */
QUILTTYPE *
quilt_plugin_serializer_first(QUILTTYPE *buf)
{
	QUILTCB *cur;

	memset(buf, 0, sizeof(QUILTTYPE));
	for(cur = cb_first; cur; cur = cur->next)
	{
		if(cur->type != QCB_SERIALIZE)
		{
			continue;
		}
		quilt_copy_quilttype_(buf, cur);
		return buf;
	}
	return NULL;
}

QUILTTYPE *quilt_plugin_next(QUILTTYPE *current)
{
	QUILTCB *cur;
	QUILTCBTYPE type;

	cur = (QUILTCB *) current->data;
	memset(current, 0, sizeof(QUILTTYPE));
	type = cur->type;
	for(cur = cur->next; cur; cur = cur->next)
	{
		if(cur->type != type)
		{
			continue;
		}
		quilt_copy_quilttype_(current, cur);
		return current;
	}
	return NULL;
}
	

/* Convert a (constant) QUILTTYPE structure to an (allocated) QUILTMIME */
static QUILTMIME *
mime_create(const QUILTTYPE *type)
{
	QUILTMIME *p;
	size_t count;
	const char *s, *t;

	if(strlen(type->mimetype) > QUILT_MIME_LEN)
	{
		quilt_logf(LOG_ERR, "internal error: specified MIME type '%s' is too long\n", type->mimetype);
		errno = EINVAL;
		return NULL;
	}
	p = (QUILTMIME *) calloc(1, sizeof(QUILTMIME));
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) sizeof(QUILTMIME));
		return NULL;
	}
	strncpy(p->mimetype, type->mimetype, QUILT_MIME_LEN);
	p->mimetype[QUILT_MIME_LEN] = 0;
	p->qs = type->qs;
	p->visible = type->visible;
	if(type->extensions)
	{
		count = 0;
		t = type->extensions;
		for(t = type->extensions; *t; t++)
		{
			if(isspace(*t))
			{
				continue;
			}
			count++;
			while(*t && !isspace(*t))
			{
				t++;
			}
			if(!*t)
			{
				break;
			}
		}
		p->extensions = (char **) calloc(count + 1, sizeof(char *));
		if(!p->extensions)
		{
			quilt_logf(LOG_CRIT, "failed to allocate extensions list for MIME type '%s'\n", type->mimetype);
			mime_destroy(p);
			return NULL;
		}
		count = 0;
		for(t = type->extensions; *t; t++)
		{
			if(isspace(*t))
			{
				continue;
			}
			s = t;
			while(*t && !isspace(*t))
			{
				t++;
			}
			p->extensions[count] = (char *) malloc(t - s + 1);
			if(!p->extensions[count])
			{
				quilt_logf(LOG_CRIT, "failed to allocate memory to duplicate file extension\n");
				mime_destroy(p);
				return NULL;
			}
			memcpy(p->extensions[count], s, t - s);
			p->extensions[count][t - s] = 0;
			quilt_logf(LOG_DEBUG, "added extension '%s' for type '%s'\n", p->extensions[count], type->mimetype);
			count++;
			if(!*t)
			{
				break;
			}
		}
	}
	if(type->desc)
	{
		p->desc = strdup(type->desc);
		if(!p->desc)
		{
			quilt_logf(LOG_CRIT, "failed to duplicate '%s'\n", type->desc);
			mime_destroy(p);
			return NULL;
		}
	}
	return p;
}

/* Destroy a QUILTMIME previously created by mime_create() */
static void
mime_destroy(QUILTMIME *mime)
{
	size_t c;

	if(mime)
	{
		if(mime->extensions)
		{
			for(c = 0; mime->extensions[c]; c++)
			{
				free(mime->extensions[c]);
			}
			free(mime->extensions);
		}
		free(mime->desc);
		free(mime);
	}
}

/* Copy the contents of a QUILTCB's QUILTMIME structure into a constant
 * QUILTTYPE used by the public interface.
 *
 * Note that only the preferred extension (if any) is returned.
 */
static void
quilt_copy_quilttype_(QUILTTYPE *dest, QUILTCB *src)
{
	memset(dest, 0, sizeof(QUILTTYPE));
	dest->data = (void *) src;
	dest->mimetype = src->mime->mimetype;
	if(src->mime->extensions)
	{
		dest->extensions = src->mime->extensions[0];
	}
	dest->desc = src->mime->desc;
	dest->visible = src->mime->visible;
	dest->qs = src->mime->qs;
}
