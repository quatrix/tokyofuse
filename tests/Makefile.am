TESTS = check_utils check_tc
check_PROGRAMS = check_utils check_tc

AM_CFLAGS = -I../src


check_utils_SOURCES = check_utils.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c
check_utils_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_utils_LDADD =  @CHECK_LIBS@

check_tc_SOURCES = check_tc.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc.h $(top_builddir)/src/tc.c
check_tc_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_LDADD =  @CHECK_LIBS@
