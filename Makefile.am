## Twine: A Linked Data workflow engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014 BBC
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

ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = m4 libsupport liburi libsparqlclient libnegotiate jsondata

SUBDIRS = libsupport @subdirs@ libnegotiate libliquify .

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport \
	-I$(top_builddir)/libnegotiate -I$(top_srcdir)/libnegotiate \
	-I$(top_builddir)/libliquify -I$(top_srcdir)/libliquify

sbin_PROGRAMS = quiltd

quiltd_SOURCES = p_quilt.h \
	main.c config.c request.c error.c urlencode.c sparql.c librdf.c fcgi.c \
	html.c \
	engine-resourcegraph.c engine-coref.c

quiltd_LDADD = \
	libsupport/libsupport.la \
	libnegotiate/libnegotiate.la \
	libliquify/libliquify.la \
	@fcgi_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	@LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@ \
	@LIBJSONDATA_LOCAL_LIBS@ @LIBJSONDATA_LIBS@

