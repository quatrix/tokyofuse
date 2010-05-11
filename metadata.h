#ifndef METADATA_H
#define METADATA_H
#include "uthash.h"

struct tc_dir_meta {
	const char *path;
	int value;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;


void add_path(const char *, int );
void remove_path(const char *); 
void free_tc_dir(tc_dir_meta_t *);
tc_dir_meta_t *lookup_path(const char *);

#endif
