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

static void quilt_canon_sort_params_(QUILTCANON *canon);
static int quilt_canon_sort_params_compare_(const void *ptra, const void *ptrb);
static int quilt_canon_del_param_(QUILTCANON *canon, const char *name, size_t start);
static char *quilt_canon_urlencode_maybe_(const char *src);

/* Create a canonical URI object, optionally copying elements from an existing
 * structure.
 */
QUILTCANON *
quilt_canon_create(QUILTCANON *source)
{
	QUILTCANON *p;
	size_t c;

	p = (QUILTCANON *) calloc(1, sizeof(QUILTCANON));
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to allocate memory for canonical URI object\n");
		return NULL;
	}
#define DUPSTR(p, dest, source, name)									\
	if(source)															\
	{																	\
		if(!(dest = strdup(source)))									\
		{																\
				quilt_logf(LOG_CRIT, "failed to duplicate " name " in canonical URI object\n"); \
				quilt_canon_destroy(p);									\
				return NULL;											\
		}																\
	}

	if(source)
	{
		DUPSTR(p, p->base, source->base, "base URL");
		DUPSTR(p, p->path, source->path, "local path");
		DUPSTR(p, p->name, source->name, "object name");
		DUPSTR(p, p->ext, source->ext, "default extension");
		DUPSTR(p, p->explicitext, source->explicitext, "explicit extension");
		DUPSTR(p, p->fragment, source->fragment, "fragment");
		if(source->nparams)
		{
			p->params = (struct quilt_canon_param_struct *) calloc(source->nparams, sizeof(struct quilt_canon_param_struct));
			if(!p->params)
			{
				quilt_logf(LOG_CRIT, "failed to allocate memory for parameters in canonical URI object\n");
				quilt_canon_destroy(p);
				return NULL;
			}
			for(c = 0; c < source->nparams; c++)
			{
				p->params[c].name = strdup(source->params[c].name);
				p->params[c].value = strdup(source->params[c].value);
				if(!p->params[c].name || !p->params[c].value)
				{
					quilt_logf(LOG_CRIT, "failed to duplicate query parameter in canonical URI object\n");
					free(p->params[c].name);
					free(p->params[c].value);
					quilt_canon_destroy(p);
					return NULL;
				}
				p->nparams++;
			}
		}
	}
	return p;
}

/* Free the resources used by a canonical URI object */
int
quilt_canon_destroy(QUILTCANON *canon)
{
	size_t c;

	free(canon->base);
	free(canon->path);
	free(canon->ext);
	free(canon->explicitext);
	free(canon->fragment);
	for(c = 0; c < canon->nparams; c++)
	{
		free(canon->params[c].name);
		free(canon->params[c].value);
	}
	free(canon->params);
	free(canon->user_path);
	free(canon->user_query);
	free(canon);
	return 0;
}

/* Set the base path/URI used of a canonical URI object */
int
quilt_canon_set_base(QUILTCANON *canon, const char *base)
{
	char *p, *t;
	size_t l;

	p = strdup(base);
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to duplicate base URI <%s>\n", base);
		return -1;
	}
	/* Strip any trailing fragments or query-strings */
	t = strchr(p, '?');
	if(t)
	{
		*t = 0;
	}
	t = strchr(p, '#');
	if(t)
	{
		*t = 0;
	}
	/* Strip any trailing slash */
	for(l = strlen(p); l && p[l - 1] == '/'; l--)
	{
		p[l - 1] = 0;
	}
	free(canon->base);
	canon->base = p;
	return 0;
}

/* Set the default (i.e., not user-specified) extension for the canonical
 * resource
 */
int
quilt_canon_set_ext(QUILTCANON *canon, const char *ext)
{
	char *p;

	while(ext && *ext == '.')
	{
		ext++;
	}
	if(ext && *ext)
	{
		p = strdup(ext);
		if(!p)
		{
			quilt_logf(LOG_CRIT, "failed to duplicate file extension '%s' in canonical URI object\n", ext);
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(canon->ext);
	canon->ext = p;
	return 0;
}

/* Set the user-specified extension for the canonical resource */
int
quilt_canon_set_explicitext(QUILTCANON *canon, const char *ext)
{
	char *p;

	while(ext && *ext == '.')
	{
		ext++;
	}
	if(ext && *ext)
	{
		p = strdup(ext);
		if(!p)
		{
			quilt_logf(LOG_CRIT, "failed to duplicate file extension '%s' in canonical URI object\n", ext);
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(canon->explicitext);
	canon->explicitext = p;
	return 0;
}

/* Set the fragment of the canonical resource */
int
quilt_canon_set_fragment(QUILTCANON *canon, const char *fragment)
{
	char *p;

	while(fragment && *fragment == '#')
	{
		fragment++;
	}
	if(fragment && *fragment)
	{
		p = strdup(fragment);
		if(!p)
		{
			quilt_logf(LOG_CRIT, "failed to duplicate fragment '#%s' in canonical URI object\n", fragment);
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(canon->fragment);
	canon->fragment = p;
	return 0;
}

/* Set the resource name -- only typically used for index documents, where
 * it's not part of the usual request path
 */
int
quilt_canon_set_name(QUILTCANON *canon, const char *name)
{
	char *p;

	if(name && *name)
	{
		p = strdup(name);
		if(!p)
		{
			quilt_logf(LOG_CRIT, "failed to duplicate resource name '%s' in canonical URI object\n", name);
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(canon->name);
	canon->name = p;
	return 0;
}

/* Reset the path components in a canonical request */
int
quilt_canon_reset_path(QUILTCANON *canon)
{
	free(canon->path);
	canon->path = NULL;
	return 0;
}

/* Append one or more path components to a canonical request */
int
quilt_canon_add_path(QUILTCANON *canon, const char *path)
{
	char *p;
	size_t l;

	while(path && *path == '/')
	{
		path++;
	}
	if(!path || !*path)
	{
		return 0;
	}
	if(canon->path)
	{
		p = (char *) realloc(canon->path, strlen(canon->path) + strlen(path) + 2);
	}
	else
	{
		p = (char *) calloc(1, strlen(path) + 1);
	}
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to resize canonical URI object's path buffer\n");
		return -1;
	}
	canon->path = p;
	p = strchr(canon->path, 0);
	if(p > canon->path)
	{
		*p = '/';
		p++;
	}
	strcpy(p, path);
	/* Strip any trailing slash */
	for(l = strlen(p); l && p[l - 1] == '/'; l--)
	{
		p[l - 1] = 0;
	}
	return 0;
}

/* Reset the query parameters of a canonical resource */
int
quilt_canon_reset_params(QUILTCANON *canon)
{
	size_t c;

	for(c = 0; c < canon->nparams; c++)
	{
		free(canon->params[c].name);
		free(canon->params[c].value);
	}
	free(canon->params);
	canon->params = NULL;
	canon->nparams = 0;
	return 0;
}
	

/* Set a multi value query parameter of a canonical resource, removing any with the
 * same name which might exist
 */
int
quilt_canon_set_param_multi(QUILTCANON *canon, const char *name, const char *values[])
{
	/* Remove all the current parameters for 'name' */
	size_t c, i = 0;
	
	for(c = 0; c < canon->nparams; c++)
	{
		if(!strcmp(canon->params[c].name, name))
		{
			/* XXX: will quilt_canon_del_param_() mean that 'c' should
			 * not be incremented?
			 */
			quilt_canon_del_param_(canon, name, c);
		}
	}

	/* Add in all the parameters from the kvset array */
	for( i = 0; values[i] != NULL; i++) 
	{
		if(quilt_canon_add_param(canon, name, values[i]))
		{
			quilt_logf(LOG_CRIT, "failed to add value for parameter '%s'\n", name);
		}
	}
	return 0;
}

/* Set a query parameter of a canonical resource, removing any with the
 * same name which might exist
 */
int
quilt_canon_set_param(QUILTCANON *canon, const char *name, const char *value)
{
	size_t c;
	char *p;

	for(c = 0; c < canon->nparams; c++)
	{
		if(!strcmp(canon->params[c].name, name))
		{
			if(value)
			{
				p = quilt_canon_urlencode_maybe_(value);
				if(!p)
				{
					return -1;
				}
				free(canon->params[c].value);
				canon->params[c].value = p;
				return quilt_canon_del_param_(canon, name, c + 1);
			}
			else
			{
				return quilt_canon_del_param_(canon, name, c);
			}
		}
	}
	return quilt_canon_add_param(canon, name, value);
}

int
quilt_canon_set_param_int(QUILTCANON *canon, const char *name, long value)
{
	char buf[64];

	snprintf(buf, sizeof(buf) - 1, "%ld", value);
	return quilt_canon_set_param(canon, name, buf);
}

/* Unconditionally add a query parameter of a canonical resource */
int
quilt_canon_add_param(QUILTCANON *canon, const char *name, const char *value)
{
	struct quilt_canon_param_struct *p;

	p = (struct quilt_canon_param_struct *) realloc(canon->params, (canon->nparams + 1) * sizeof(struct quilt_canon_param_struct));
	if(!p)
	{
		quilt_logf(LOG_CRIT, "failed to add parameter '%s' to canonical URI object\n", name);
		return -1;
	}
	canon->params = p;
	p = &(canon->params[canon->nparams]);
	memset(p, 0, sizeof(struct quilt_canon_param_struct));
	p->name = strdup(name);
	if(!p->name)
	{
		quilt_logf(LOG_CRIT, "failed to set name of parameter '%s' in canonical URI object\n");
		return -1;
	}
	if(!value)
	{
		value = "";
	}
	p->value = quilt_canon_urlencode_maybe_(value);
	if(!p->value)
	{
		return -1;
	}
	canon->nparams++;
	quilt_canon_sort_params_(canon);
	return 0;
}

int
quilt_canon_add_param_int(QUILTCANON *canon, const char *name, long value)
{
	char buf[64];
	
	snprintf(buf, sizeof(buf) - 1, "%ld", value);
	return quilt_canon_set_param(canon, name, buf);
}

int
quilt_canon_set_user_path(QUILTCANON *canon, const char *path)
{
	char *p;

	if(!path)
	{
		free(canon->user_path);
		canon->user_path = NULL;
		return 0;
	}
	while(*path == '/')
	{
		path++;
	}
	p = strdup(path);
	if(!p)
	{
		return -1;
	}
	free(canon->user_path);
	canon->user_path = p;
	p = strchr(canon->user_path, '?');
	if(p)
	{
		*p = 0;
	}
	return 0;
}

int
quilt_canon_set_user_query(QUILTCANON *canon, const char *query)
{
	char *p;

	if(!query)
	{
		free(canon->user_query);
		canon->user_query = NULL;
		return 0;
	}
	if(*query == '?')
	{
		query++;
	}
	if(!*query)
	{
		free(canon->user_query);
		canon->user_query = NULL;
		return 0;
	}
	p = strdup(query);
	if(!p)
	{
		return -1;
	}
	free(canon->user_query);
	canon->user_query = p;
	return 0;
}

/* Serialise a canonical URI object to a string. The returned buffer must be
 * released with free() when no longer required by the caller.
 */
char *
quilt_canon_str(QUILTCANON *canon, QUILTCANOPTS opts)
{
	size_t l, c;
	char *s, *p;

	/* If QCO_FORCEEXT is provided, it overrides QCO_NOEXT */
	if(opts & QCO_FORCEEXT)
	{
		opts &= ~QCO_NOEXT;
	}
	/* If there's an explicit extension, ensure the name is used if there
	 * is one.
	 */
	if(!(opts & QCO_NOEXT) && (canon->explicitext || ((opts & QCO_FORCEEXT) && canon->ext)))
	{
		opts |= QCO_NAME;
	}
	l = 0;
	l += (opts & QCO_NOABSOLUTE) ? 0 : (canon->base ? strlen(canon->base) : 0);
	/* Trailing slash */
	l++;
	l += (opts & QCO_NOPATH) ? 0 : (canon->path ? strlen(canon->path) : 0);
	/* Trailing slash */
	l++;
	l += (opts & QCO_NAME) ? (canon->name ? strlen(canon->name) : 0) : 0;
	l += canon->explicitext ? strlen(canon->explicitext) + 1 : 0;
	l += canon->ext ? strlen(canon->ext) + 1 : 0;
	l += (opts & QCO_NOPARAMS) ? 0 : 0;
	l += (opts & QCO_FRAGMENT) ? (canon->fragment ? strlen(canon->fragment) + 1 : 0) : 0;
	/* User-agent-supplied values */
	if(opts & QCO_USERSUPPLIED)
	{
		l += (canon->user_path ? strlen(canon->user_path) + 1 : 0);
		l += (canon->user_query ? strlen(canon->user_query) + 1 : 0);
	}
	if(!(opts & QCO_NOPARAMS))
	{
		l++;
		for(c = 0; c < canon->nparams; c++)
		{
			l += strlen(canon->params[c].name);
			l++;
			l += strlen(canon->params[c].value);
			l++;
		}
	}
	/* Null terminator */
	l++;
	s = (char *) calloc(1, l);
	if(!s)
	{
		quilt_logf(LOG_CRIT, "failed to allocate buffer of %lu bytes to serialise canonical URI object\n", (unsigned long ) l);
		return NULL;
	}
	p = s;
	if(!(opts & QCO_NOABSOLUTE) && canon->base)
	{
		strcpy(p, canon->base);
		p = strchr(p, 0);
	}
	*p = '/';
	p++;
	if((opts & QCO_USERSUPPLIED) && !(opts & QCO_NOPATH) && canon->user_path)
	{
		strcpy(p, canon->user_path);
		p = strchr(p, 0);
	}
	else
	{
		if(!(opts & QCO_NOPATH) && canon->path)
		{
			strcpy(p, canon->path);
			p = strchr(p, 0);
		}
		if((opts & QCO_NAME) && canon->name)
		{
			if(!(opts & QCO_NOPATH) && canon->path)
			{
				*p = '/';
				p++;
			}
			strcpy(p, canon->name);
			p = strchr(p, 0);
		}
		if(opts & QCO_FORCEEXT)
		{
			if(canon->ext)
			{
				*p = '.';
				p++;
				strcpy(p, canon->ext);
				p = strchr(p, 0);
			}
			else if(canon->explicitext)
			{
				*p = '.';
				p++;
				strcpy(p, canon->explicitext);
				p = strchr(p, 0);
			}
		}
		else if(!(opts & QCO_NOEXT) && canon->explicitext)
		{
			*p = '.';
			p++;
			strcpy(p, canon->explicitext);
			p = strchr(p, 0);		
		}
	}
	if((opts & QCO_USERSUPPLIED) && !(opts & QCO_NOPARAMS) && canon->user_query)
	{
		*p = '?';
		p++;
		strcpy(p, canon->user_query);
		p = strchr(p, 0);
	}
	else if(!(opts & QCO_NOPARAMS) && canon->nparams)
	{
		*p = '?';
		p++;
		for(c = 0; c < canon->nparams; c++)
		{
			strcpy(p, canon->params[c].name);
			p = strchr(p, 0);
			*p = '=';
			p++;
			strcpy(p, canon->params[c].value);
			p = strchr(p, 0);
			*p = '&';
			p++;
		}
		/* Back up one character to overwrite the trailing ampersand */
		p--;		
	}
	if((opts & QCO_FRAGMENT) && canon->fragment)
	{
		*p = '#';
		p++;
		strcpy(p, canon->fragment);
		p = strchr(p, 0);
	}
	*p = 0;
	return s;
}

static int
quilt_canon_del_param_(QUILTCANON *canon, const char *name, size_t start)
{
	for(; start < canon->nparams; start++)
	{
		if(strcmp(name, canon->params[start].name))
		{
			break;
		}
		free(canon->params[start].name);
		free(canon->params[start].value);
		if(start + 1 < canon->nparams)
		{
			memmove(&(canon->params[start]), &(canon->params[start + 1]), sizeof(struct quilt_canon_param_struct) * (canon->nparams - start - 1));
		}
		canon->nparams--;
	}
	return 0;
}

static void
quilt_canon_sort_params_(QUILTCANON *canon)
{
	qsort(canon->params, canon->nparams, sizeof(struct quilt_canon_param_struct), quilt_canon_sort_params_compare_);
}

static int
quilt_canon_sort_params_compare_(const void *ptra, const void *ptrb)
{
	const struct quilt_canon_param_struct *a, *b;
	int r;

	a = (struct quilt_canon_param_struct *) ptra;
	b = (struct quilt_canon_param_struct *) ptrb;
	r = strcmp(a->name, b->name);
	if(!r)
	{
		r = strcmp(a->value, b->value);
	}
	return r;
}

/* Selectively URL-encode a string. We use this where the string is supposed
 * to have been URL-encoded already, but we want to be sure. In the perfect
 * case, the return value is the equivalent of strdup(src).
 *
 * Specifically:
 *	 '%' is only encoded if it is not followed by two hex chars
 *	 '&', '#', and '=' are always encoded
 *	 ' ' is encoded to '+'
 */
static char *
quilt_canon_urlencode_maybe_(const char *src)
{
	static const char digits[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f',
	};
	size_t l;
	const char *t;
	char *buf, *p;
	int ch;

	l = 1;
	for(t = src; *t; t++)
	{
		ch = (int) ((unsigned char) (*t));
		l++;
		if(ch == '&' || ch == '#' || ch == ' ')
		{
			l += 2;
			continue;
		}
		if(ch == '%')
		{
			if(!isxdigit(t[1]) || !isxdigit(t[2]))
			{
				l += 2;
			}
			continue;
		}
		if(!isprint(ch) || ch > 127)
		{
			l += 2;
			continue;
		}
	}
	buf = (char *) calloc(1, l);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, "failed to allocate %lu bytes for URL-encoded string buffer\n", (unsigned long) l);
		return NULL;
	}
	p = buf;
	for(t = src; *t; t++)
	{
		ch = (int) ((unsigned char) (*t));
		if(ch == ' ')
		{
			*p = '+';
			p++;
			continue;
		}
		if(ch == '%' && isxdigit(t[1]) && isxdigit(t[2]))
		{
			*p = '%';
			p++;
			continue;
		}
		if(ch != '&' && ch != '#' && ch != ' ' &&
			 isprint(ch) && ch <= 127)
		{
			*p = ch;
			p++;
			continue;
		}
		*p = '%';
		p++;
		*p = digits[(ch & 0xf0) >> 4];
		p++;
		*p = digits[(ch & 0x0f)];
		p++;
	}
	*p = 0;
	return buf;
}
