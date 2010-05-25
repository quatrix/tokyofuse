#ifndef METADATA_H
#define METADATA_H
#include "uthash.h"
#include <tchdb.h>


#define TC_UP_REFCOUNT 1
#define TC_DONT_UP_REFCOUNT 0

#define TC_GC_SLEEP 5 // 5 seconds between every gc run

struct tc_file_meta {
	char *path;
	size_t size;
};


struct tc_dir_meta {
	const char *path;
//	struct tc_file_meta *files;
	int refcount;
	TCHDB *hdb;
	pthread_mutex_t lock;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;
typedef struct tc_file_meta tc_file_meta_t;


int init_metadata(void);
tc_dir_meta_t *add_path(const char *);
tc_dir_meta_t *open_tc(const char *);
int release_path(const char *); 
int release_file(const char *);
void free_tc_dir(tc_dir_meta_t *);
void free_tc_file(tc_file_meta_t *);
tc_dir_meta_t *lookup_path(const char *);
tc_dir_meta_t *init_tc_dir(const char *);
int add_to_meta_hash(tc_dir_meta_t *);
int meta_filesize(const char *);
int tc_filesize(const char *);
char *tc_value(const char *, int *);

tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *, tc_file_meta_t *);

int tc_dir_lock(tc_dir_meta_t *);
int tc_dir_unlock(tc_dir_meta_t *);
int datalock_lock(void);
int datalock_unlock(void);
int metalock_read_lock(void);
int metalock_write_lock(void);
int metalock_unlock(void);

size_t unique_id(void);

void tc_gc(void *);

char *get_caller(void);

#endif
