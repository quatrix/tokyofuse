#include <pthread.h>
#include <stdlib.h>
#include "tc.h"
#include "tc_backend.h"
#include "utils.h"
#include "tc_dir.h"



tc_dir_meta_t *tc_dir_allocate(const char *path)
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

	tc_dir->path 	 	= strdup(path);
	tc_dir->hdb      	= NULL; 
	tc_dir->refcount 	= 0;
	tc_dir->initialized = 0;

	if (tc_dir->path == NULL) {
		error("can't allocate memory for tc_dir->path");
		goto tc_dir_free;
	}

	if (pthread_mutex_init(&tc_dir->lock, NULL) != 0) {
		error("can't init rwlock for tc_dir");
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
	size_t path_len = strlen(tc_dir->path);
	char tc_path[path_len + TC_PREFIX_LEN + 1];

	to_tc_path(tc_dir->path, tc_path);

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

	if (!tc_dir_lock(tc_dir))
		return 0;
	
	tc_dir->refcount--;

	tc_dir_unlock(tc_dir);

	debug("refcount for %s decremented (refcount: %d)", tc_dir->path, tc_dir->refcount);
	return 1;
}


inline int tc_dir_lock(tc_dir_meta_t *tc_dir)
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

	if (pthread_mutex_lock(&tc_dir->lock) == 0) {
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

	if (pthread_mutex_trylock(&tc_dir->lock) == 0) {
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
		pthread_mutex_unlock(&tc_dir->lock);
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

		if (tc_dir->path != NULL) {
			free((char *)tc_dir->path);
			tc_dir->path = NULL;
		}

		tc_dir_unlock(tc_dir);
		pthread_mutex_destroy(&tc_dir->lock);

		free(tc_dir);
	}
	else
		error("asked to free a NULL tc_dir");
}

