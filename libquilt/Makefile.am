## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2015 BBC
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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libnegotiate -I$(top_srcdir)/libnegotiate

include_HEADERS = libquilt.h libquilt-sapi.h

lib_LTLIBRARIES = libquilt.la

libquilt_la_SOURCES = p_libquilt.h \
	init.c log.c config.c error.c librdf.c request.c sparql.c urlencode.c \
	plugin.c canon.c

libquilt_la_LDFLAGS = -avoid-version -no-undefined

libquilt_la_LIBADD = $(top_builddir)/libnegotiate/libnegotiate.la \
	@LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@
