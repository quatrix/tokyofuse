#ifndef METADATA_H
#define METADATA_H
#include <tchdb.h>
#include "uthash.h"


#define TC_UP_REFCOUNT 1
#define TC_DONT_UP_REFCOUNT 0

#define TC_GC_SLEEP 5 // 5 seconds between every gc run

#define TC_CABINET_TRIES 500 // retries for tchdb* functions
#define TC_CABINET_USLEEP 30 // micro seconds

#define TC_RETRY_LOOP(hdb, path, cond, on_failure) 																\
do {																											\
	int i = 0, ecode = 0;																						\
																												\
	while (!(cond)) {																							\
		ecode = tchdbecode((hdb));																				\
																												\
		fprintf(stderr, "%s error: %s (%s) [%d/%d]\n", #cond, tchdberrmsg(ecode), (path), i, TC_CABINET_TRIES);	\
																												\
		if (ecode != TCEMMAP || ++i == TC_CABINET_TRIES) {														\
			on_failure;																							\
			break;																								\
		}																										\
																												\
		wake_up_gc();																							\
		usleep(TC_CABINET_USLEEP);																				\
	}																											\
																												\
} while (0)

struct tc_file_meta {
	char *path;
	size_t size;
};

typedef struct tc_file_meta tc_file_meta_t;

struct tc_dir_meta {
	const char *path;
	struct tc_file_meta *files;
	int refcount;
	int initialized;
	pthread_mutex_t lock;
	TCHDB *hdb;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;

struct tc_filehandle {
	char *value;
	int value_len;
	tc_dir_meta_t *tc_dir;
};

typedef struct tc_filehandle tc_filehandle_t;


typedef enum {
	TC_LOCK_WRITE  		= 1,
	TC_LOCK_READ		= 2,
	TC_LOCK_DONT_UNLOCK = 4
} TC_LOCKTYPE;

typedef enum {
	TC_ERROR,
	TC_EXISTS,
	TC_NOT_FOUND
} TC_RC;

int init_metadata(void);

tc_dir_meta_t *open_tc(const char *);
int release_path(tc_dir_meta_t *);
int tc_filesize(const char *);
int tc_value(const char *, tc_filehandle_t *);

tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *, tc_file_meta_t *);

void tc_gc(void *);



#endif
