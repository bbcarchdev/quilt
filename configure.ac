## Quilt: A Linked Open Data server
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

AC_INIT([quilt],[master])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC
AC_PROG_CC_C99
BT_PROG_CC_WARN

LT_INIT

BT_ENABLE_XSI
BT_ENABLE_POSIX

BT_REQUIRE_LIBDL
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBURI_INCLUDED
BT_REQUIRE_LIBSPARQLCLIENT_INCLUDED
BT_REQUIRE_LIBJSONDATA_INCLUDED

AC_CHECK_HEADERS([strings.h])

AC_ARG_WITH([fcgi],[AS_HELP_STRING(--with-fcgi=PREFIX)],[fcgi_prefix="$withval"],[fcgi_prefix="yes"])
if test x"$fcgi_prefix" = x"yes" ; then
	fcgi_prefix=""
fi
if test x"$fcgi_prefix" = x"no" ; then
   AC_MSG_ERROR([Quilt cannot be built without fcgi])
fi

if ! test x"$fcgi_prefix" = x"" ; then
	fcgi_CFLAGS="-I$fcgi_prefix/include"
	fcgi_LDFLAGS="-L$fcgi_prefix/lib"
fi
fcgi_LIBS="-lfcgi"
WITH_FASTCGI=1
AC_DEFINE_UNQUOTED([WITH_FASTCGI],[1],[Define if FastCGI is available])

AC_SUBST([fcgi_LIBS])
AC_SUBST([fcgi_CFLAGS])
AC_SUBST([fcgi_LDFLAGS])
AC_SUBST([fcgi_SUBDIRS])
AM_CONDITIONAL([WITH_FASTCGI],[test x"$WITH_FASTCGI" = x"1"])

BT_DEFINE_PREFIX

AC_CONFIG_FILES([
Makefile
libsupport/Makefile
m4/Makefile
libnegotiate/Makefile
libliquify/Makefile
templates/Makefile
public/Makefile
libquilt/Makefile
engines/Makefile
serialisers/Makefile
apache2-example.conf
])

AC_OUTPUT
