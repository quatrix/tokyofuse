#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uthash.h"
#include "metadata.h"


struct tc_dir_meta *meta = NULL;


void add_path(const char *path, int value)
{
	fprintf(stderr, "adding %s to metadata hash\n", path);
	
	if (lookup_path(path) != NULL) {
		fprintf(stderr, "key %s already in metadata hash\n", path);
	}

	tc_dir_meta_t *tc_dir = (tc_dir_meta_t *)malloc(sizeof(tc_dir_meta_t));
	tc_dir->path = path;
	tc_dir->value = value;

	HASH_ADD_KEYPTR( hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir );
}

tc_dir_meta_t *lookup_path(const char *path) 
{
	tc_dir_meta_t *tc_dir;

    HASH_FIND_STR( meta, path, tc_dir );  
    return tc_dir;
}

void remove_path(const char *path) 
{
	tc_dir_meta_t *tc_dir;

	tc_dir = lookup_path(path);

	if (tc_dir != NULL) {
		fprintf(stderr, "removing path %s\n", path);
		HASH_DEL( meta, tc_dir);
		free_tc_dir(tc_dir);
	}
}

void free_tc_dir(tc_dir_meta_t *tc_dir)
{
	if (tc_dir != NULL)
		free(tc_dir);
}
