#ifndef METADATA_H
#define METADATA_H
#include <tchdb.h>
#include "uthash.h"


#define TC_UP_REFCOUNT 1
#define TC_DONT_UP_REFCOUNT 0

#define TC_GC_SLEEP 5 // 5 seconds between every gc run

#define TC_CABINET_TRIES 500
#define TC_CABINET_USLEEP 30 // micro seconds


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
	TC_LOCK_WRITE, 
	TC_LOCK_READ
} TC_LOCKTYPE;



int init_metadata(void);

tc_dir_meta_t *allocate_tc_dir(const char *);
tc_dir_meta_t *init_tc_dir(tc_dir_meta_t *);

tc_dir_meta_t *add_path(const char *);
tc_dir_meta_t *lookup_path(const char *);
tc_dir_meta_t *open_tc(const char *);


int release_path(tc_dir_meta_t *);

void free_tc_dir(tc_dir_meta_t *);
void free_tc_file(tc_file_meta_t *);


int meta_filesize(const char *);
int tc_filesize(const char *);
int tc_dir_get_filesize(tc_dir_meta_t *, const char *);
int tc_value(const char *, tc_filehandle_t *);
int create_file_hash(tc_dir_meta_t *);

tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *, tc_file_meta_t *);




void tc_gc(void *);

char *get_caller(void);

#endif
