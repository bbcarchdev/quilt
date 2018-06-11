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

DIST_SUBDIRS = m4 libsupport libnegotiate libliquify libkvset \
	templates public libquilt servers engines serialisers \
	sample-data docbook-html5 docs

SUBDIRS = libsupport @subdirs@ libnegotiate libliquify libkvset \
	libquilt \
	servers engines serialisers \
	templates public sample-data . docs

EXTRA_DIST = libquilt.pc.in libquilt-uninstalled.pc.in

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libquilt.pc

noinst_DATA = libquilt-uninstalled.pc.in

dist_doc_DATA = quilt.conf

install-data-hook:
	$(INSTALL) -m 755 -d "$(DESTDIR)$(sysconfdir)"
	test -f "$(DESTDIR)$(sysconfdir)/quilt.conf" || $(INSTALL) -m 644 "$(srcdir)/quilt.conf" "$(DESTDIR)$(sysconfdir)"

BRANCH ?= develop
DOCS_BRANCH ?= gh-pages

DEVELOP_SUBMODULES = m4 libsupport
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
