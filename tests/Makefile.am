TESTS = check_utils check_tc check_tc_backend check_tc_gc check_tc_dir check_metadata
check_PROGRAMS = check_utils check_tc check_tc_backend check_tc_gc check_tc_dir check_metadata

AM_CFLAGS = -I../src
src_dir = $(top_builddir)/src

check_utils_SOURCES = check_utils.c $(src_dir)/utils.h $(src_dir)/utils.c
check_utils_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_utils_LDADD =  @CHECK_LIBS@

check_tc_SOURCES = check_tc.c $(src_dir)/utils.h $(src_dir)/utils.c $(src_dir)/tc.h $(src_dir)/tc.c
check_tc_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_LDADD =  @CHECK_LIBS@

check_tc_backend_SOURCES = check_tc_backend.c $(src_dir)/utils.h $(src_dir)/utils.c $(src_dir)/tc_backend.c $(src_dir)/tc_backend.h
check_tc_backend_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_backend_LDADD =  @CHECK_LIBS@ -ltokyocabinet

check_tc_gc_SOURCES = check_tc_gc.c $(src_dir)/utils.h $(src_dir)/utils.c $(src_dir)/tc_gc.h $(src_dir)/tc_gc.c
check_tc_gc_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_gc_LDADD =  @CHECK_LIBS@

check_tc_dir_SOURCES = check_tc_dir.c $(src_dir)/utils.h $(src_dir)/utils.c $(src_dir)/tc_dir.h $(src_dir)/tc_dir.c $(src_dir)/tc.c $(src_dir)/tc.h
check_tc_dir_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_tc_dir_LDADD =  @CHECK_LIBS@

check_metadata_SOURCES = check_metadata.c $(src_dir)/utils.h $(src_dir)/utils.c $(src_dir)/tc_backend.c $(src_dir)/tc_backend.h $(src_dir)/metdata.h $(src_dir)/tc.c $(src_dir)/tc.h $(src_dir)/metadata.c $(src_dir)/metadata.h $(src_dir)/tc_dir.c $(src_dir)/tc_dir.h
check_metadata_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_metadata_LDADD =  @CHECK_LIBS@ -ltokyocabinet
