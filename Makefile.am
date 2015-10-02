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

ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = m4 libsupport libnegotiate \
	templates public libquilt engines serialisers \
	sample-data docbook-html5 docs

SUBDIRS = libsupport @subdirs@ libnegotiate libliquify \
	templates public libquilt engines serialisers sample-data . docs

EXTRA_DIST = libquilt.pc.in libquilt-uninstalled.pc.in

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libquilt.pc

noinst_DATA = libquilt-uninstalled.pc.in

doc_DATA = apache2-example.conf

dist_doc_DATA = quilt.conf

AM_CPPFLAGS = @AM_CPPFLAGS@ @fcgi_CFLAGS@ \
	-I$(top_builddir)/libquilt -I$(top_srcdir)/libquilt \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport \
	-I$(top_builddir)/libnegotiate -I$(top_srcdir)/libnegotiate

bin_PROGRAMS = quilt

quilt_SOURCES = p_cli.h cli.c

quilt_LDFLAGS = -R$(libdir)

quilt_LDADD = \
	libquilt/libquilt.la \
	libsupport/libsupport.la \
	libnegotiate/libnegotiate.la \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	@LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@

if WITH_FASTCGI

sbin_PROGRAMS = quilt-fcgid

quilt_fcgid_SOURCES = p_fcgi.h fcgi.c

# On some systems, while $(libdir) appears in the system library search list,
# the loader is unable to find libraries installed there without an
# intermediate step being performed. Using -R$(libdir) prevents the issue
# from occurring (by including a DT_RPATH hint, or equivalent in the quiltd
# executable), but packaging scripts may wish to use chrpath(1) or similar
# to strip it out of packaged binaries.
quilt_fcgid_LDFLAGS = -R$(libdir) @fcgi_LDFLAGS@

quilt_fcgid_LDADD = \
	libquilt/libquilt.la \
	libsupport/libsupport.la \
	libnegotiate/libnegotiate.la \
	@fcgi_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	@LIBSPARQLCLIENT_LOCAL_LIBS@ @LIBSPARQLCLIENT_LIBS@

endif

install-data-hook:
	$(INSTALL) -m 755 -d "$(DESTDIR)$(sysconfdir)"
	test -f "$(DESTDIR)$(sysconfdir)/quilt.conf" || $(INSTALL) -m 644 "$(srcdir)/quilt.conf" "$(DESTDIR)$(sysconfdir)"

BRANCH ?= develop
DOCS_BRANCH ?= gh-pages

DEVELOP_SUBMODULES = m4 libsupport libs3client
DOCS_SUBMODULES = docbook-html5 docs

checkout:
	@for submod in $(DEVELOP_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(BRANCH) && \
		git pull && \
		$(MAKE) checkout BRANCH=$(BRANCH) || exit $? && \
		cd .. ; \
	done
	@for submod in $(DOCS_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(DOCS_BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(DOCS_BRANCH) && \
		git pull && \
		cd .. ; \
	done
