#ifndef METADATA_H
#define METADATA_H
#include "uthash.h"
#include <tchdb.h>


#define TC_UP_REFCOUNT 1
#define TC_DONT_UP_REFCOUNT 0

#define TC_GC_SLEEP 5 // 5 seconds between every gc run

struct tc_file_meta {
	const char *path;
	size_t size;
	UT_hash_handle hh;
};

struct tc_dir_meta {
	const char *path;
	struct tc_file_meta *files;
	int refcount;
	TCHDB *hdb;
	pthread_mutex_t lock;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;
typedef struct tc_file_meta tc_file_meta_t;


int init_metadata(void);
tc_dir_meta_t *add_path(const char *);
tc_dir_meta_t *open_tc(const char *, int);
int release_path(const char *); 
int release_file(const char *);
void free_tc_dir(tc_dir_meta_t *);
tc_dir_meta_t *lookup_path(const char *);
tc_dir_meta_t *init_tc_dir(const char *, int);
tc_file_meta_t *lookup_file(tc_dir_meta_t *, const char *);
int add_to_meta_hash(tc_dir_meta_t *);
int meta_filesize(const char *);
int tc_filesize(const char *);
char *tc_value(const char *, int *);
int create_file_hash(tc_dir_meta_t *);
void print_file_hash(tc_file_meta_t *);
void tc_gc(void *);

#endif
