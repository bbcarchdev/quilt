/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
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

#include "p_libnegotiate.h"

static const char *neg_negotiate_next_(const char **ptr);
static float neg_parse_q_(const char *qval);
static void neg_apply_(struct negotiate_entry_struct *entry, const char *type, size_t len, float q);
static void neg_apply_type_(struct negotiate_entry_struct *entry, const char *type, size_t len, float q);
static void neg_check_match_(NEGOTIATE *neg, struct negotiate_entry_struct *entry);

/* Create a negotiation object */
NEGOTIATE *
neg_create(void)
{
	NEGOTIATE *p;
	
	p = (NEGOTIATE *) calloc(1, sizeof(NEGOTIATE));
	if(!p)
	{
		return NULL;
	}
	return p;
}

/* Destroy a negotiation object */
void
neg_destroy(NEGOTIATE *neg)
{
	size_t c;
	
	for(c = 0; c < neg->nentries; c++)
	{
		free(neg->entries[c].name);
	}
	free(neg->entries);
	free(neg);
}

/* Add a value and associated qs-value to the list */
int
neg_add(NEGOTIATE *neg, const char *name, float qs)
{
	struct negotiate_entry_struct *p;
	size_t c;

	if(qs > 1)
	{
		qs = 1.0f;
	}
	if(qs < 0)
	{
		qs = 0.0f;
	}
	for(c = 0; c < neg->nentries; c++)
	{
		if(!strcasecmp(neg->entries[c].name, name))
		{
/*			fprintf(stderr, "negotiate: replacing %s (%f) -> (%f)\n", name, neg->entries[c].qs, qs); */
			neg->entries[c].qs = qs;
			return 0;
		}
	}
	p = (struct negotiate_entry_struct *) realloc(neg->entries, sizeof(struct negotiate_entry_struct) * (neg->nentries + 1));
	if(!p)
	{
		return -1;
	}
	neg->entries = p;
	p[neg->nentries].name = strdup(name);
	if(!p[neg->nentries].name)
	{
		return -1;
	}
	p[neg->nentries].qs = qs;
	neg->nentries++;
	return 0;
}

/* Perform HTTP-style negotiation for a single-level value, such as
 * character set or language. For media types, see neg_negotiate_type().
 */
const char *
neg_negotiate(NEGOTIATE *neg, const char *accept)
{
	size_t c;
	const char *start, *p, *e, *end;
	float q;
	
	/* Reset everything */
	neg->matched = NULL;
	neg->q = 0;
	for(c = 0; c < neg->nentries; c++)
	{
		neg->entries[c].qw = 0;
		neg->entries[c].qp = 0;
		neg->entries[c].q = 0;
	}
	/* Be forgiving */
	while(isspace(*accept))
	{
		accept++;
	}
	end = accept;
	/* While there are tokens... */
	while((start = neg_negotiate_next_(&end)))
	{
		q = 1;
		e = NULL;
		for(p = start; p < end; p++)
		{
			if(*p == ';')
			{
				if(!e)
				{
					e = p;
				}
				/* We don't need to do bounds-checking here because
				 * the terminating byte won't match the conditions,
				 * and the end of a single token (as defined by
				 * neg_negotiate_next_()) won't either.
				 */
				if(p[1] == 'q' && p[2] == '=')
				{
					q = neg_parse_q_(&p[3]);
					break;
				}
			}
		}
		if(!e)
		{
			e = end;
		}
		/* Now the type name lies in {start..e} */
		for(c = 0; c < neg->nentries; c++)
		{
			neg_apply_(&(neg->entries[c]), start, e - start, q);
		}
	}
	/* Now find the best match */
	for(c = 0; c < neg->nentries; c++)
	{
		neg_check_match_(neg, &(neg->entries[c]));
	}
	if(neg->matched)
	{
		return neg->matched->name;
	}
	return NULL;
}

/* Perform HTTP-style negotiation for a content type (i.e., a
 * a two-level value set -- such as text/plain, or audio/mpeg)
 */
const char *
neg_negotiate_type(NEGOTIATE *neg, const char *accept)
{
	size_t c;
	const char *start, *p, *e, *end;
	float q;
	
	/* Reset everything */
	neg->matched = NULL;
	neg->q = 0;
	for(c = 0; c < neg->nentries; c++)
	{
		neg->entries[c].qw = 0;
		neg->entries[c].qp = 0;
		neg->entries[c].q = 0;
	}
	/* Be forgiving */
	while(isspace(*accept))
	{
		accept++;
	}
	end = accept;
	/* While there are tokens... */
	while((start = neg_negotiate_next_(&end)))
	{
		q = 1;
		e = NULL;
		for(p = start; p < end; p++)
		{
			if(*p == ';')
			{
				if(!e)
				{
					e = p;
				}
				/* We don't need to do bounds-checking here because
				 * the terminating byte won't match the conditions,
				 * and the end of a single token (as defined by
				 * neg_negotiate_next_()) won't either.
				 */
				if(p[1] == 'q' && p[2] == '=')
				{
					q = neg_parse_q_(&p[3]);
					break;
				}
			}
		}
		if(!e)
		{
			e = end;
		}
		/* Now the type name lies in {start..e} */
		for(c = 0; c < neg->nentries; c++)
		{
			neg_apply_type_(&(neg->entries[c]), start, e - start, q);
		}
	}
	/* Now find the best match */
	for(c = 0; c < neg->nentries; c++)
	{
		/* fprintf(stderr, "negotiate: checking [%s] (qs=%f, qw=%f, qp=%f, q=%f)\n", neg->entries[c].name, neg->entries[c].qs, neg->entries[c].qw, neg->entries[c].qp, neg->entries[c].q); */
		neg_check_match_(neg, &(neg->entries[c]));
	}
	if(neg->matched)
	{
		/* fprintf(stderr, "negotiate: negotiated [%s] at (%f)\n", neg->matched->name, neg->q); */
		return neg->matched->name;
	}
	return NULL;
}

/* Return a pointer to the start of the next token, updating *ptr to
 * point to the end of that token.
 */
static const char *
neg_negotiate_next_(const char **ptr)
{
	const char *start;
	
	/* Skip any padding/empty entries */
	while(isspace(**ptr) || **ptr == ',')
	{
		(*ptr)++;
	}
	/* End of the line */
	if(!**ptr)
	{
		return NULL;
	}
	start = *ptr;
	while(**ptr)
	{
		if(isspace(**ptr) || **ptr == ',')
		{
			return start;
		}
		(*ptr)++;
	}
	return start;
}

/* Parse a q-value */
static float
neg_parse_q_(const char *qval)
{
	float q;
	
	while(*qval && isspace(*qval))
	{
		qval++;
	}
	q = 1;
	if(*qval == '0')
	{
		q = 0;
		qval++;
	}
	if(*qval != '.')
	{
		return q;
	}
	qval++;
	q = 0;
	if(*qval >= '0' && *qval <= '9')
	{
		q += (*qval - '0') * 100;
		qval++;
		if(*qval >= '0' && *qval <= '9')
		{
			q += (*qval - '0') * 10;
			qval++;
			if(*qval >= '0' && *qval <= '9')
			{
				q += (*qval - '0');
			}
		}
	}
	return q / 1000;
}

/* Apply the q-value to an entry when negotiating content type */
static void
neg_apply_type_(struct negotiate_entry_struct *entry, const char *type, size_t len, float q)
{
	const char *s;
	
	if(len == 3 && type[0] == '*' && type[1] == '/' && type[2] == '*')
	{
		/* Wildcard */
		entry->qw = entry->qs * q;
		return;
	}
	if(strlen(entry->name) == len && !strncasecmp(entry->name, type, len))
	{
		/* Exact match */
		entry->q = entry->qs * q;
		return;
	}
	for(s = type; s < type + len; s++)
	{
		if(s[0] == '/' && (len - (s - type)) == 2 && s[1] == '*')
		{
			/* Partial wildcard */
			s++;
			if(!strncasecmp(entry->name, type, s - type))
			{
				entry->qp = entry->qs * q;
			}
			return;
		}
	}
}

/* Apply the q-value to an entry when negotiating single-level values */
static void
neg_apply_(struct negotiate_entry_struct *entry, const char *type, size_t len, float q)
{
	if(len == 1 && type[0] == '*')
	{
		/* Wildcard */
		entry->qw = entry->qs * q;
		return;
	}
	if(strlen(entry->name) == len && !strncasecmp(entry->name, type, len))
	{
		/* Exact match */
		entry->q = entry->qs * q;
		return;
	}
}

/* Compare the given entry against neg->matched and update it if
 * the entry is better-ranked
 */
static void
neg_check_match_(NEGOTIATE *neg, struct negotiate_entry_struct *entry)
{
	if(entry->q && entry->q > neg->q)
	{
		neg->matched = entry;
		neg->q = entry->q;
		return;
	}
	if(entry->qp && entry->qp > neg->q)
	{
		neg->matched = entry;
		neg->q = entry->qp;
		return;
	}
	if(entry->qw && entry->qw > neg->q)
	{
		neg->matched = entry;
		neg->q = entry->qw;
		return;
	}
}
