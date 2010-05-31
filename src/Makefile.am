FLAGS=-O3 --std=gnu99 -Wall -Werror -g
AM_LDFLAGS = `pkg-config --cflags --libs fuse` $(FLAGS) -ltokyocabinet -lz -ltap
AM_CFLAGS = `pkg-config --cflags fuse`$(FLAGS)

bin_PROGRAMS = tokyofuse
tokyofuse_SOURCES = metadata.c metadata.h tc_backend.c tc_backend.h tc.c tc_gc.c tc_gc.h tc.h tokyofuse.c uthash.h utils.c utils.h


