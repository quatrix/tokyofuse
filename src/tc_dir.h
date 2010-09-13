#ifndef TC_DIR_H
#define TC_DIR_H
#include <tchdb.h>
#include "uthash.h"
#include "tc.h"

//#define USE_SPINLOCK 

struct tc_dir_meta {
	char path[MAX_PATH_LEN];
	size_t path_len;
	int refcount;
	int initialized;
#ifdef USE_SPINLOCK
	pthread_spinlock_t lock;
#else
	pthread_mutex_t lock;
#endif
	TCHDB *hdb;
    UT_hash_handle hh; 
};

typedef struct tc_dir_meta tc_dir_meta_t;

tc_dir_meta_t *tc_dir_allocate(const char *, size_t);
tc_dir_meta_t *tc_dir_init(tc_dir_meta_t *);
inline int tc_dir_dec_refcount(tc_dir_meta_t *);
void tc_dir_free(tc_dir_meta_t *);
inline int tc_dir_lock(tc_dir_meta_t *);
inline int tc_dir_trylock(tc_dir_meta_t *);
inline int tc_dir_unlock(tc_dir_meta_t *);

#endif
