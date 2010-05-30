#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include "uthash.h"
#include "metadata.h"
#include "utils.h"
#include "tc_gc.h"
#include "tc_backend.h"
#include "tc_dir.h"
#include "tc.h"

#define LOCK_DEBUG 1
#define DEBUG 1

static tc_dir_meta_t *meta = NULL;
static pthread_rwlock_t meta_lock;


#if LOCK_DEBUG
static char *str_locktype[] = { "", "write", "read", "", "", "write + dont_unlock", "read + dont_unlock" };
#endif

static inline void add_to_meta_hash(tc_dir_meta_t *);
static inline int metalock_lock(TC_LOCKTYPE);
static inline int metalock_unlock(void);
static TC_RC lookup_path(const char *, tc_dir_meta_t **, TC_LOCKTYPE);
static void free_tc_file(tc_file_meta_t *);


int init_metadata(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		debug("can't init rwlock meta_lock\n");
		return -errno;
	}

	return 0;
}


tc_dir_meta_t *get_tc(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;
	
	debug("adding %s to metadata hash\n", path);

	if (lookup_path(path, &tc_dir, TC_LOCK_READ) != TC_NOT_FOUND) {
		return tc_dir;
	}

	// if key doesn't exist, lock for write and check again, 
	// since read locks allow multiple readers, 
	// so multiple threads could be waiting for this write lock
	// and one of them could have already created the key
	
	if (lookup_path(path, &tc_dir, TC_LOCK_WRITE | TC_LOCK_DONT_UNLOCK ) != TC_NOT_FOUND)
		goto return_tc;

	if ((tc_dir = tc_dir_allocate(path)) == NULL)
		goto return_tc;

	add_to_meta_hash(tc_dir);
	
	metalock_unlock();

	if (tc_dir_init(tc_dir))
		debug("returning new tc_dir %s (initial refcount %d)\n", path,  tc_dir->refcount);
	else
		debug("failed to initialize tc_dir %s\n", path);

	tc_dir_unlock(tc_dir);

	return tc_dir->initialized ? tc_dir : NULL;

return_tc:
	metalock_unlock();

	return tc_dir; 
}


static inline void add_to_meta_hash(tc_dir_meta_t *tc_dir)
{
	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);
}





tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *tc_dir, tc_file_meta_t *last_tc_file)
{
	tc_file_meta_t *tc_file = NULL;
	const char *fetched_data;
	char *current_key = NULL;
	char *last_key = last_tc_file == NULL ? NULL : last_tc_file->path;
	int last_key_len = last_key == NULL ? 0 : strlen(last_key);

	int fetched_data_len = 0;
	int current_key_len = 0;

	current_key = tc_get_next(tc_dir->hdb, tc_dir->path, 
					last_key, last_key_len, 
					&current_key_len, &fetched_data,  
					&fetched_data_len);

	free_tc_file(last_tc_file);

	if (current_key != NULL) {
		tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));

		if (tc_file == NULL) {
			debug("can't allocate memory for new tc_file\n");
			return NULL;
		}

		tc_file->path = strndup(current_key, current_key_len);
		tc_file->size = fetched_data_len;

		free(current_key);
	}

	return tc_file;
}



static void free_tc_file(tc_file_meta_t *tc_file)
{
	if (tc_file != NULL) {
		if (tc_file->path != NULL)
			free(tc_file->path);

		free(tc_file);
	}
}

static TC_RC lookup_path(const char *path, tc_dir_meta_t **tc_dir_ptr, TC_LOCKTYPE lock)
{
	TC_RC rc = TC_ERROR;
	tc_dir_meta_t *tc_dir = *tc_dir_ptr;

	debug("looking up %s\n", path);

	if (!metalock_lock(lock))
		return rc;

	HASH_FIND_STR(meta, path, tc_dir);

	if (tc_dir != NULL) {
		debug("found %s in hash\n", path);

		tc_dir_lock(tc_dir);

		if (tc_dir->initialized) {
			tc_dir->refcount++;
			debug("key %s already in metadata hash (refcount incs to %d)\n", path, tc_dir->refcount);
			*tc_dir_ptr = tc_dir;
			rc = TC_EXISTS;
		}
		else 
			debug("found %s in hash - but it's not initialized\n", path);

		tc_dir_unlock(tc_dir);
	}
	else 
		rc = TC_NOT_FOUND;

	if (!(lock & TC_LOCK_DONT_UNLOCK))
		metalock_unlock();

	return rc;
}


int tc_filesize(const char *path)
{
	int size;
	char *parent = parent_path(path);
	const char *leaf = leaf_file(path);
	tc_dir_meta_t *tc_dir;

	debug("fetching filesize for %s/%s\n", parent, leaf);
	
	tc_dir = get_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return -1;


	size = tc_get_filesize(tc_dir->hdb, tc_dir->path, leaf);



	if (!tc_dir_dec_refcount(tc_dir))
		return -1;
	
	debug("fetched filesize for %s is %d\n", leaf, size);
	return size;
}



int tc_value(const char *path, tc_filehandle_t *fh)
{
	tc_dir_meta_t *tc_dir;
	char *value = NULL;
	int value_len = 0;
	char *parent = parent_path(path);
	const char *leaf = leaf_file(path);

	debug("fetching value for %s/%s\n", parent, leaf);

	tc_dir = get_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return 0;

	if ((value = tc_get_value(tc_dir->hdb, tc_dir->path, leaf, &value_len)) == NULL) {
		tc_dir_dec_refcount(tc_dir);
		return 0;
	}

	fh->value = value;
	fh->value_len = value_len;
	fh->tc_dir = tc_dir;

	debug("fetched %s size: %d\n", leaf, value_len);

	return 1;
}


int release_path(tc_dir_meta_t *tc_dir)
{
	int rc = 0;

	if (tc_dir == NULL) {
		debug("tried to release path on NULL tc_dir\n");
		return 0;
	}

	if (!metalock_lock(TC_LOCK_READ))
		return 0;

	if (tc_dir_dec_refcount(tc_dir))
		rc = 1;

	metalock_unlock();

	return rc;
}






static inline int metalock_lock(TC_LOCKTYPE locktype)
{
#if LOCK_DEBUG
	size_t uid = unique_id();
	char *caller = get_caller();
#endif 
	int rc = 0;
	int l_rc = -1;
	
#if LOCK_DEBUG
	debug("locking meta (req: %u caller: %s type: %s)\n", uid, caller, str_locktype[locktype]);
#endif

	if (locktype && TC_LOCK_WRITE) 
		l_rc = pthread_rwlock_wrlock(&meta_lock);
	else if (locktype && TC_LOCK_READ)
		l_rc = pthread_rwlock_rdlock(&meta_lock);
	else {
		debug("unknown lock type specified\n");
		goto free_caller;
	}

	if (l_rc == 0) {
#if LOCK_DEBUG
		debug("locking meta (res: %u caller: %s type: %s)\n", uid, caller, str_locktype[locktype]);
#endif
		rc = 1;
		goto free_caller;
	}
	else {
		debug("unable to lock meta\n");
		goto free_caller;
	}

free_caller:
#if LOCK_DEBUG
	if (caller != NULL)
		free(caller);
#endif

	return rc;
}


static inline int metalock_unlock(void)
{
#if LOCK_DEBUG
	char *caller = NULL;
	caller = get_caller();
	debug("unlocking metalock (caller: %s)\n",  caller);
	if (caller != NULL)
		free(caller);
#endif

	if (pthread_rwlock_unlock(&meta_lock) != 0) {
		debug("can't unlock metalock\n");
		return 0;
			
	}

	return 1;
}



void remove_unused_tc_dir(void)
{
	tc_dir_meta_t *tc_dir = NULL;
	tc_dir_meta_t *tc_dir_next = NULL;

	for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir_next) {
		// in case we free tc_dir
		tc_dir_next = tc_dir->hh.next;

		if (pthread_rwlock_trywrlock(&meta_lock) == 0) {
			if (pthread_mutex_trylock(&tc_dir->lock) == 0) {

				if (tc_dir->refcount ==  0) { 
					debug("remove_unused_tc_dir: refcount for %s is 0 -- freeing it\n", tc_dir->path);

					HASH_DEL(meta, tc_dir);

					// FIXME: can't unlock metalock here:
					// must wait for free_tc to finish 
					// new hdb might be opened before current closes (threading error)

					tc_dir_free(tc_dir); 
				} 
				else 
					tc_dir_unlock(tc_dir);
			}
			
			metalock_unlock();
		}

	}

}
