#ifndef METADATA_H
#define METADATA_H
#include "uthash.h"
#include <tchdb.h>

struct tc_file_meta {
	const char *path;
	size_t size;
	UT_hash_handle hh;
};

struct tc_dir_meta {
	const char *path;
	struct tc_file_meta *files;
	int refcount;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;
typedef struct tc_file_meta tc_file_meta_t;


int init_metadata(void);
tc_dir_meta_t *add_path(const char *);
int remove_path(const char *); 
void free_tc_dir(tc_dir_meta_t *);
tc_dir_meta_t *lookup_path(const char *);
tc_file_meta_t *lookup_file(tc_dir_meta_t *, const char *);
int meta_filesize(const char *);
int tc_filesize(const char *);
char *tc_value(const char *, int *);
int create_file_hash(TCHDB *, tc_dir_meta_t *);
void print_file_hash(tc_file_meta_t *);

#endif
