#ifndef TC_DIR_H
#define TC_DIR_H
#include "uthash.h"
#include <tchdb.h>

struct tc_dir_meta {
	const char *path;
	int refcount;
	int initialized;
	pthread_mutex_t lock;
	TCHDB *hdb;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;

tc_dir_meta_t *tc_dir_allocate(const char *);
tc_dir_meta_t *tc_dir_init(tc_dir_meta_t *);
inline int tc_dir_dec_refcount(tc_dir_meta_t *);
void tc_dir_free(tc_dir_meta_t *);
inline int tc_dir_lock(tc_dir_meta_t *);
inline int tc_dir_unlock(tc_dir_meta_t *);

#endif
