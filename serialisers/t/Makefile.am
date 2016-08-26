AM_CFLAGS=-I${top_srcdir} -I/usr/include-std=c99 -Wall
AM_LDFLAGS = -L/usr/include -lcunit -ljansson

AM_CPPFLAGS = @AM_CPPFLAGS@ \
        -I$(top_builddir)/libquilt -I$(top_srcdir)/libquilt \
        -I$(top_builddir)/libliquify -I$(top_srcdir)/libliquify \
        -I$(top_builddir)/serialisers -I$(top_srcdir)/serialisers

LIBS = @LIBS@

check_PROGRAMS = test_model

test_model_SOURCES = $(top_builddir)/serialisers/model.h test_model.c

test_model_LDADD = $(top_builddir)/serialisers/html.la \
	$(top_builddir)/libquilt/libquilt.la \
        $(top_builddir)/libliquify/libliquify.la \
        @LIBJANSSON_LOCAL_LIBS@ @LIBJANSSON_LIBS@

TESTS = $(check_PROGRAMS)
