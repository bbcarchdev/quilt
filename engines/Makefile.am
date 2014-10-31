## Quilt: A Linked Data API web application
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

AM_CPPFLAGS = @AM_CPPFLAGS@ -I$(top_srcdir)/libquilt
LIBS = $(top_builddir)/libquilt/libquilt.la

moduledir = $(libdir)/quilt

module_LTLIBRARIES = coref.la resourcegraph.la

coref_la_SOURCES = p_coref.h coref.c
coref_la_LDFLAGS = -module -no-undefined -avoid-version
coref_la_LIBADD = @LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@

resourcegraph_la_SOURCES = p_resourcegraph.h resourcegraph.c
resourcegraph_la_LDFLAGS = -module -no-undefined -avoid-version
resourcegraph_la_LIBADD = @LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@