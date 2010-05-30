TESTS = check_utils check_tc check_tc_backend check_tc_gc check_tc_dir
check_PROGRAMS = check_utils check_tc check_tc_backend check_tc_gc check_tc_dir

AM_CFLAGS = -I../src


check_utils_SOURCES = check_utils.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c
check_utils_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_utils_LDADD =  @CHECK_LIBS@

check_tc_SOURCES = check_tc.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc.h $(top_builddir)/src/tc.c
check_tc_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_LDADD =  @CHECK_LIBS@

check_tc_backend_SOURCES = check_tc_backend.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc_backend.c $(top_builddir)/src/tc_backend.h
check_tc_backend_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_backend_LDADD =  @CHECK_LIBS@ -ltokyocabinet

check_tc_gc_SOURCES = check_tc_gc.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc_gc.h $(top_builddir)/src/tc_gc.c
check_tc_gc_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_gc_LDADD =  @CHECK_LIBS@

check_tc_dir_SOURCES = check_tc_dir.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc_dir.h $(top_builddir)/src/tc_dir.c $(top_builddir)/src/tc.c $(top_builddir)/src/tc.h
check_tc_dir_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_dir_LDADD =  @CHECK_LIBS@

#check_metadata_SOURCES = check_metadata.c $(top_builddir)/src/utils.h $(top_builddir)/src/utils.c $(top_builddir)/src/tc_backend.c $(top_builddir)/src/tc_backend.h $(top_builddir)/src/metdata.h $(top_builddir)/src/tc_gc.c $(top_builddir)/src/tc_gc.h $(top_builddir)/src/tc.c $(top_builddir)/src/tc.h
#check_metadata_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
#check_metadata_LDADD =  @CHECK_LIBS@ -ltokyocabinet
