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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libquilt -I$(top_srcdir)/libquilt \
	-I$(top_builddir)/libliquify -I$(top_srcdir)/libliquify

moduledir = $(libdir)/quilt

module_LTLIBRARIES = html.la text.la

html_la_SOURCES = p_html.h html.c html-types.c html-classes.c
html_la_LIBADD = $(top_builddir)/libquilt/libquilt.la \
	$(top_builddir)/libliquify/libliquify.la \
	@LIBJANSSON_LOCAL_LIBS@ @LIBJANSSON_LIBS@

html_la_LDFLAGS = -module -no-undefined -avoid-version

text_la_SOURCES = p_text.h text.c
text_la_LIBADD = $(top_builddir)/libquilt/libquilt.la \
	@LIBRAPTOR2_LOCAL_LIBS@ @LIBRAPTOR2_LIBS@
text_la_LDFLAGS = -module -no-undefined -avoid-version
