##  Author: Mo McRoberts
##
##  Copyright (c) 2014 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

noinst_LTLIBRARIES = libliquify.la

libliquify_la_SOURCES = libliquify.h p_libliquify.h \
	env.c parse.c token.c expression.c dump.c apply.c blocks.c \
	tags.c filters.c \
	tag-include.c \
	block-for.c block-if.c \
	filter-escape.c filter-case.c

libliquify_la_LIBADD = $(LIBJANSSON_LOCAL_LIBS) $(LIBJANSSON_LIBS)

noinst_PROGRAMS = liquify-parse liquify-apply

liquify_parse_SOURCES = liquify-parse.c
liquify_parse_LDADD = libliquify.la $(LIBJANSSON_LOCAL_LIBS) $(LIBJANSSON_LIBS)

liquify_apply_SOURCES = liquify-apply.c
liquify_apply_LDADD = libliquify.la $(LIBJANSSON_LOCAL_LIBS) $(LIBJANSSON_LIBS)
