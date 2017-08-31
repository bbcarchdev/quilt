## Quilt: A Linked Data API web application
##
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

AM_CPPFLAGS = @AM_CPPFLAGS@ @fcgi_CFLAGS@ \
	-I$(top_builddir)/libquilt -I$(top_srcdir)/libquilt \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport \
	-I$(top_builddir)/libnegotiate -I$(top_srcdir)/libnegotiate \
	-I$(top_builddir)/libkvset -I$(top_srcdir)/libkvset

bin_PROGRAMS = quilt

quilt_SOURCES = p_cli.h cli.c

quilt_LDFLAGS = -R$(libdir)

quilt_LDADD = \
	libquilt/libquilt.la \
	libsupport/libsupport.la \
	libnegotiate/libnegotiate.la \
	libkvset/libkvset.la \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	@LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@ \
	@PTHREAD_LOCAL_LIBS@ @PTHREAD_LIBS@
