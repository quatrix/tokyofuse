#include <pthread.h>
#include <stdlib.h>
#include "tc.h"
#include "tc_backend.h"
#include "utils.h"
#include "tc_dir.h"



tc_dir_meta_t *tc_dir_allocate(const char *path, size_t path_len)
{
	tc_dir_meta_t *tc_dir	= NULL;

	if (path == NULL) {
		error("tc_dir_allocate got null path");
		return NULL;
	}

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL) {
		error("can't allocate memory for tc_dir_meta_t struct");
		return NULL;
	}

	if (!s_strncpy(tc_dir->path, path, path_len, MAX_PATH_LEN)) {
		error("can't copy path into tc_dir->path");
		goto tc_dir_free;
	}

	tc_dir->path_len	= path_len;
	tc_dir->hdb      	= NULL; 
	tc_dir->refcount 	= 0;
	tc_dir->initialized = 0;

	if (tc_dir->path == NULL) {
		error("can't allocate memory for tc_dir->path");
		goto tc_dir_free;
	}

#ifdef USE_SPINLOCK
	if (pthread_spin_init(&tc_dir->lock, 0) != 0) {
#else
	if (pthread_mutex_init(&tc_dir->lock, NULL) != 0) {
#endif
		error("can't init lock for tc_dir");
		goto tc_dir_free;
	}

	if (!tc_dir_lock(tc_dir))
		goto tc_dir_free;

	return tc_dir;

tc_dir_free:
	tc_dir_free(tc_dir);
	return NULL;
}

tc_dir_meta_t *tc_dir_init(tc_dir_meta_t *tc_dir) 
{
	//size_t path_len = strlen(tc_dir->path);
	//char tc_path[path_len + TC_PREFIX_LEN + 1];
	char tc_path[MAX_PATH_LEN];

	if (to_tc_path(tc_dir->path, tc_dir->path_len, tc_path) == NULL) {
		error("path %s longer than %d bytes", tc_dir->path, MAX_PATH_LEN);
		return NULL;
	}

	if ((tc_dir->hdb = tc_open(tc_path)) == NULL)
		return NULL;

	// if all successful, raise refcount
	tc_dir->refcount 	= 1; 
	tc_dir->initialized	= 1;

	return tc_dir;
}


inline int tc_dir_dec_refcount(tc_dir_meta_t *tc_dir)
{
	if (tc_dir == NULL) {
		error("tried to refrence dec a null tc_dir");
		return 0;
	}

	int r; 
	//if (!tc_dir_lock(tc_dir))
	//	return 0;
	
	//tc_dir->refcount--;
	r = __sync_fetch_and_sub(&tc_dir->refcount, 1);

	//tc_dir_unlock(tc_dir);

	debug("refcount for %s decremented (refcount: %d)", tc_dir->path, tc_dir->refcount);
	return 1;
}


inline int tc_dir_lock(tc_dir_meta_t *tc_dir)
{
	if (tc_dir == NULL) {
		error("unable to lock tc_dir - it's null");
		return 0;
	}

	//double t0, td;

	//t0 = get_time();

#if LOCK_DEBUG
	size_t uid = unique_id();
	char *caller = get_caller();
#endif
	int rc = 0;

#if LOCK_DEBUG
	debug("locking tc_dir %s (req: %u caller: %s)", tc_dir->path, uid, caller);
#endif

#ifdef USE_SPINLOCK
	if (pthread_spin_lock(&tc_dir->lock) == 0) {
#else
	if (pthread_mutex_lock(&tc_dir->lock) == 0) {
#endif

#if LOCK_DEBUG
		debug("locking tc_dir %s (res: %u caller: %s)", tc_dir->path, uid, caller); 
#endif
		rc = 1;
	}
	else 
		error("unable to lock tc_dir");

#if LOCK_DEBUG
	if (caller != NULL)
		free(caller);
#endif 
/* 
	td = (get_time() - t0) * 1000;

	if (td > 10) 
		error("getting tc_dir lock took: %0.3f msec (rc: %d)", td,rc);
*/
	return rc;
}

inline int tc_dir_trylock(tc_dir_meta_t *tc_dir)
{
	if (tc_dir == NULL) {
		error("unable to lock tc_dir - it's null");
		return 0;
	}

#if LOCK_DEBUG
	size_t uid = unique_id();
	char *caller = get_caller();
#endif
	int rc = 0;

#if LOCK_DEBUG
	debug("locking tc_dir %s (req: %u caller: %s)", tc_dir->path, uid, caller);
#endif

#ifdef USE_SPINLOCK
	if (pthread_spin_trylock(&tc_dir->lock) == 0) {
#else
	if (pthread_mutex_trylock(&tc_dir->lock) == 0) {
#endif

#if LOCK_DEBUG
		debug("locking tc_dir %s (res: %u caller: %s)", tc_dir->path, uid, caller); 
#endif
		rc = 1;
	}
	else {
#if LOCK_DEBUG
		debug("unable to lock tc_dir %s (res: %u caller: %s)", tc_dir->path, uid, caller); 
#endif
	}

#if LOCK_DEBUG
	if (caller != NULL)
		free(caller);
#endif 

	return rc;
}

inline int tc_dir_unlock(tc_dir_meta_t *tc_dir)
{

#if LOCK_DEBUG
	char *caller = NULL;
#endif

	if (tc_dir != NULL) {
#if LOCK_DEBUG
		caller = get_caller();
		debug("unlocking tc_dir %s (caller: %s)", tc_dir->path, caller);
#endif

#ifdef USE_SPINLOCK
		pthread_spin_unlock(&tc_dir->lock);
#else
		pthread_mutex_unlock(&tc_dir->lock);
#endif

#if LOCK_DEBUG
		if (caller != NULL)
			free(caller);
#endif
		return 1;
	}
	else 
		error("tried tc_dir_unlock on null tc_dir");

	return 0;
}
void tc_dir_free(tc_dir_meta_t * tc_dir)
{
	if (tc_dir != NULL) {
		debug("freeing tc_dir %s", tc_dir->path);

		if (tc_dir->hdb != NULL) {
			debug("closing hdb (%s)", tc_dir->path);

			tc_close(tc_dir->hdb, tc_dir->path);

			tc_dir->hdb = NULL;
		}

		tc_dir_unlock(tc_dir);
#ifdef USE_SPINLOCK
		pthread_spin_destroy(&tc_dir->lock);
#else
		pthread_mutex_destroy(&tc_dir->lock);
#endif

		free(tc_dir);
	}
	else
		error("asked to free a NULL tc_dir");
}

