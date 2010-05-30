FLAGS=`pkg-config --cflags --libs fuse`
FLAGS_O=`pkg-config --cflags fuse`
#PROFILE=-pg -fprofile-arcs -ftest-coverage
#CALLTRACE=-rdynamic
OPTIMIZE=-O3
CC=gcc --std=gnu99 -Wall -g $(OPTIMIZE) $(CALLTRACE) $(PROFILE)
CC_O=gcc --std=gnu99 -Wall -g -c $(OPTIMIZE) $(CALLTRACE) $(PROFILE)



all: tokyofuse testsuite

utils.o: utils.c utils.h
	$(CC_O) utils.c -o utils.o

tc.o: tc.c tc.h
	$(CC_O) tc.c -o tc.o

tc_gc.o: tc_gc.c tc_gc.h
	$(CC_O) tc_gc.c -o tc_gc.o

tc_backend.o: tc_backend.c tc_backend.h
	$(CC_O) tc_backend.c -o tc_backend.o

metadata.o: metadata.c metadata.h
	$(CC_O) metadata.c -o metadata.o

testsuite: utils.o tc_backend.o tc_gc.o tc.o metadata.o testsuite.c
	$(CC) -ltap -ltokyocabinet utils.o tc_backend.o tc_gc.o tc.o metadata.o testsuite.c -o testsuite

tokyofuse: utils.o tc_backend.o tc_gc.o tc.o metadata.o tokyofuse.c
	$(CC) $(FLAGS) -ltokyocabinet -lz tc_backend.o tc_gc.o tc.o utils.o metadata.o tokyofuse.c -o tokyofuse

test: testsuite
	./testsuite

clean:
	rm -f *.o testsuite tokyofuse	

clean_gprof:
	rm -f gmon.out *.gcda *.gcno
