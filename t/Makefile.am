AM_CFLAGS=-I${top_srcdir} -I/usr/include-std=c99 -Wall
AM_LDFLAGS = -L/usr/include -lcunit -ljansson

AM_CPPFLAGS = @AM_CPPFLAGS@ \
        -I$(top_builddir)/libquilt -I$(top_srcdir)/libquilt

LIBS = @LIBS@

check_PROGRAMS = test_fcgi

test_fcgi_SOURCES = $(top_builddir)/p_fcgi.h test_fcgi.c

TESTS = $(check_PROGRAMS)
