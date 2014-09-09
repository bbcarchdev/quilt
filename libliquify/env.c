/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC.
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

#include "p_libliquify.h"

static void liquify_logger_(int level, const char *fmt, va_list ap);

/* Create a liquify environment */
LIQUIFY *
liquify_create(void)
{
	LIQUIFY *p;

	p = (LIQUIFY *) calloc(1, sizeof(LIQUIFY));
	if(!p)
	{
		fprintf(stderr, "libliquify: failed to allocate %lu bytes: %s\n", (unsigned long) sizeof(LIQUIFY), strerror(errno));
		return NULL;
	}
	p->vlogf = liquify_logger_;
	return p;
}

/* Set the logging callback for use in a liquify environment.
 * Pass NULL as the callback to reset to the default logger.
 */
int
liquify_set_logger(LIQUIFY *liquify, void (*logger)(int level, const char *fmt, va_list ap))
{
	if(!logger)
	{
		logger = liquify_logger_;
	}
	liquify->vlogf = logger;
	return 0;
}

/* Set the loader callback (and user data pointer) to be used in a liquify
 * environment. The callback will be invoked when liquify_load() needs to
 * load a template which hasn't yet been loaded into the environment
 * (including through parsing of the {% include ... %} tag).
 *
 * The loader is responsible for locating and reading the file (or
 * equivalent), passing it to liquify_parse(), and returning the parsed
 * LIQUIFYTPL which will be used as the result of the call to liquify_load().
 */
int
liquify_set_loader(LIQUIFY *liquify, LIQUIFYTPL *(*loader)(LIQUIFY *env, const char *name, void *dta), void *data)
{
	if(loader)
	{
		liquify->loader = loader;
		liquify->loaddata = data;
	}
	else
	{
		liquify->loader = NULL;
		liquify->loaddata = NULL;
	}
	return 0;
}

/* Load a template, if it hasn't already been loaded into the environment,
 * using the callback set via liquify_set_loader().
 */
LIQUIFYTPL *
liquify_load(LIQUIFY *liquify, const char *name)
{
	LIQUIFYTPL *tpl;
	
	tpl = liquify_locate(liquify, name);
	if(tpl)
	{
		liquify_logf(liquify, LOG_DEBUG, "template '%s' has already been loaded\n", name);
		/* Template has already been loaded */
		return tpl;
	}
	if(liquify->loader)
	{
		/* Invoke the loader callback */
		return liquify->loader(liquify, name, liquify->loaddata);
	}
	liquify_logf(liquify, LOG_ERR, "cannot load template '%s': no loader has been provided\n", name);
	return NULL;
}

/* Destroy a liquify environment, freeing resources used by any loaded
 * templates.
 */
int
liquify_destroy(LIQUIFY *liquify)
{
	LIQUIFYTPL *tpl;
	
	while(liquify->first)
	{
		tpl = liquify->first->next;
		liquify_tpl_free_(liquify->first);
		liquify->first = tpl;
	}
	free(liquify);
	return 0;
}

/* Log a message in an environment. If a callback has been defined by
 * liquify_set_logger(), it will be used.
 */
void
liquify_logf(LIQUIFY *liquify, int level, const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	liquify_vlogf(liquify, level, fmt, ap);
}

void
liquify_vlogf(LIQUIFY *liquify, int level, const char *fmt, va_list ap)
{
	liquify->vlogf(level, fmt, ap);
}

/* Allocate a zero-filled buffer of len bytes, logging a critical error and
 * aborting the process if it fails.
 */
void *
liquify_alloc(LIQUIFY *liquify, size_t len)
{
	void *ptr;

	ptr = calloc(1, len);
	if(!ptr)
	{
		liquify_logf(liquify, LOG_CRIT, "failed to allocate %lu bytes: %s\n", (unsigned long) len, strerror(errno));
		abort();
	}
	return ptr;
}

/* Resize a buffer to len bytes, logging a critical error and aborting the
 * process if it fails. Note that if the new buffer is larger than the old
 * one, the extension bytes won't be zero-filled.
 */
void *
liquify_realloc(LIQUIFY *restrict liquify, void *restrict ptr, size_t newlen)
{

	ptr = realloc(ptr, newlen);   
	if(!ptr)
	{
		liquify_logf(liquify, LOG_CRIT, "failed to re-allocate %lu bytes: %s\n", (unsigned long) newlen, strerror(errno));
		abort();
	}
	return ptr;
}

/* Allocate a buffer and copy the contents of a string, logging a critical
 * error and aborting the process if allocation fails.
 */
char *
liquify_strdup(LIQUIFY *restrict liquify, const char *src)
{
	char *ptr;
	size_t l;

	l = strlen(src);
	ptr = (char *) liquify_alloc(liquify, l + 1);
	memcpy(ptr, src, l);
	ptr[l] = 0;
	return ptr;
}

/* Free a buffer previously allocated by liquify_alloc() or liquify_strdup() */
void
liquify_free(LIQUIFY *restrict liquify, void *restrict ptr)
{
	(void) liquify;

	free(ptr);
}

/* A simple default logger */
static void
liquify_logger_(int level, const char *fmt, va_list ap)
{
	fprintf(stderr, "<%d> ", level);
	vfprintf(stderr, fmt, ap);	
}
