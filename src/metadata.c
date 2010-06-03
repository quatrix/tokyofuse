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
#include "tc_backend.h"
#include "tc_dir.h"
#include "tc.h"

#define LOCK_DEBUG 1
#define DEBUG 1

static tc_dir_meta_t *meta = NULL;
static pthread_rwlock_t meta_lock;


#if LOCK_DEBUG

#define ADD_LOCK(dst, i, lock) strcat(dst, #lock "|")
#define ADD_LOCK_BITWISE(dst, i, lock) strcat(dst, (i & lock ? #lock "|" : ""))

char *metadata_lock_str(TC_LOCKTYPE lock)
{	
	size_t max_lock_str_len = strlen(TC_LOCK_ALL) + 2; 
	size_t lock_str_len;
	char *lock_str = (char *)calloc(sizeof(char), max_lock_str_len);

	ADD_LOCK(lock_str, lock, TC_LOCK_READ);
	ADD_LOCK_BITWISE(lock_str, lock, TC_LOCK_WRITE);
	ADD_LOCK_BITWISE(lock_str, lock, TC_LOCK_DONT_UNLOCK);
	ADD_LOCK_BITWISE(lock_str, lock, TC_LOCK_TRY);

	lock_str_len = strlen(lock_str);
	lock_str[lock_str_len-1] = '\0';

	return lock_str;
}

#endif



int metadata_init(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		debug("can't init rwlock meta_lock");
		return 0;
	}

	return 1;
}


tc_dir_meta_t *metadata_get_tc(const char *path)
{
	if (path == NULL)
		return NULL;

	tc_dir_meta_t *tc_dir = NULL;
	
	debug("adding %s to metadata hash", path);

	if (metadata_lookup_path(path, &tc_dir, TC_LOCK_READ) != TC_NOT_FOUND) {
		return tc_dir;
	}

	// if key doesn't exist, lock for write and check again, 
	// since read locks allow multiple readers, 
	// so multiple threads could be waiting for this write lock
	// and one of them could have already created the key
	
	if (metadata_lookup_path(path, &tc_dir, TC_LOCK_WRITE | TC_LOCK_DONT_UNLOCK ) != TC_NOT_FOUND)
		goto return_tc;

	if ((tc_dir = tc_dir_allocate(path)) == NULL)
		goto return_tc;

	if (!metadata_add_to_hash(tc_dir))
		goto destroy_tc_dir;
	
	metadata_unlock();

	if (tc_dir_init(tc_dir))
		debug("returning new tc_dir %s (initial refcount %d)", path,  tc_dir->refcount);
	else
		debug("failed to initialize tc_dir %s", path);

	tc_dir_unlock(tc_dir);

	return tc_dir->initialized ? tc_dir : NULL;

destroy_tc_dir:
	tc_dir_free(tc_dir); 
	tc_dir = NULL;

return_tc:
	metadata_unlock();

	return tc_dir; 
}


int metadata_add_to_hash(tc_dir_meta_t *tc_dir)
{
	tc_dir_meta_t *__tc_dir = NULL;

	if (tc_dir == NULL)
		return 0;

	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);

	// checking that it actually inserted
	// FIXME: not sure if it's necessery
	HASH_FIND_STR(meta, tc_dir->path, __tc_dir);

	if (tc_dir == __tc_dir)
		return 1;

	debug("failed to insert tc_dir %s to meta hash", tc_dir->path);

	return 0;
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
			debug("can't allocate memory for new tc_file");
			return NULL;
		}

		tc_file->path = strndup(current_key, current_key_len);
		tc_file->size = fetched_data_len;

		free(current_key);
	}

	return tc_file;
}

void free_tc_file(tc_file_meta_t *tc_file)
{
	if (tc_file != NULL) {
		if (tc_file->path != NULL)
			free(tc_file->path);

		free(tc_file);
	}
}

TC_RC metadata_lookup_path(const char *path, tc_dir_meta_t **tc_dir_ptr, TC_LOCKTYPE lock)
{
	TC_RC rc = TC_ERROR;
	tc_dir_meta_t *tc_dir = NULL;

	*tc_dir_ptr = NULL;

	debug("looking up %s", path);

	if (!metadata_lock(lock))
		return rc;

	HASH_FIND_STR(meta, path, tc_dir);

	if (tc_dir != NULL) {
		debug("found %s in hash", path);

		if (!tc_dir_lock(tc_dir)) 
			goto error;

		if (tc_dir->initialized) {
			tc_dir->refcount++;
			debug("key %s already in metadata hash (refcount incs to %d)", path, tc_dir->refcount);
			*tc_dir_ptr = tc_dir;
			rc = TC_EXISTS;
		}
		else {
			debug("found %s in hash - but it's not initialized", path);
			rc = TC_NOT_FOUND;
		}

		tc_dir_unlock(tc_dir);
	}
	else 
		rc = TC_NOT_FOUND;

error:

	if (!(lock & TC_LOCK_DONT_UNLOCK))
		metadata_unlock();

	return rc;
}


int metadata_get_filesize(const char *path)
{
	int size;
	char *parent = NULL;
	char *leaf = NULL;
	tc_dir_meta_t *tc_dir;

	if (path == NULL) {
		debug("metadata_get_filesize: null path");
		return -1;
	}
	
	if ((parent = parent_path(path)) == NULL) {
		debug("metadata_get_filesize: null parent");
		return -1;
	}

	if ((leaf = leaf_file(path)) == NULL) {
		debug("metadata_get_filesize: null leaf");
		return -1;
	}

	debug("fetching filesize for %s/%s", parent, leaf);
	
	tc_dir = metadata_get_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return -1;

	size = tc_get_filesize(tc_dir->hdb, tc_dir->path, leaf);

	if (!tc_dir_dec_refcount(tc_dir))
		return -1;
	
	debug("fetched filesize for %s is %d", leaf, size);
	return size;
}



int metadata_get_value(const char *path, tc_filehandle_t *fh)
{
	tc_dir_meta_t *tc_dir;
	char *value = NULL;
	char *parent = NULL;
	char *leaf = NULL;
	int value_len = 0;

	if (path == NULL) {
		debug("metadata_get_value: null path");
		return 0;
	}

	if (fh == NULL) {
		debug("metadata_get_value: null fh");
		return 0;
	}
	
	if ((parent = parent_path(path)) == NULL) {
		debug("metadata_get_value: null parent");
		return 0;
	}

	if ((leaf = leaf_file(path)) == NULL) {
		debug("metadata_get_value: null leaf");
		return 0;
	}

	debug("fetching value for %s/%s", parent, leaf);

	tc_dir = metadata_get_tc(parent);
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

	debug("fetched %s size: %d", leaf, value_len);

	return 1;
}


int metadata_release_path(tc_dir_meta_t *tc_dir)
{
	int rc = 0;

	if (tc_dir == NULL) {
		debug("tried to release path on NULL tc_dir");
		return 0;
	}

	if (!metadata_lock(TC_LOCK_READ))
		return 0;

	if (tc_dir_dec_refcount(tc_dir))
		rc = 1;

	metadata_unlock();

	return rc;
}


inline int metadata_lock(TC_LOCKTYPE locktype)
{
#if LOCK_DEBUG
	size_t uid = unique_id();
	char *caller = get_caller();
	char *lock_str = metadata_lock_str(locktype);
#endif 
	int rc = 0;
	int l_rc = -1;
	
#if LOCK_DEBUG
	debug("locking meta (req: %u caller: %s type: %s)", uid, caller, lock_str);
#endif

	if (!(locktype & TC_LOCK_WRITE))
		l_rc = locktype & TC_LOCK_TRY ? pthread_rwlock_tryrdlock(&meta_lock) : pthread_rwlock_rdlock(&meta_lock);
	else
		l_rc = locktype & TC_LOCK_TRY ? pthread_rwlock_trywrlock(&meta_lock) : pthread_rwlock_wrlock(&meta_lock);

	if (l_rc == 0) {
#if LOCK_DEBUG
		debug("locking meta (res: %u caller: %s type: %s)", uid, caller, lock_str);
#endif
		rc = 1;
		goto free_caller;
	}
	else {
#if LOCK_DEBUG
		debug("locking meta failed (res: %u caller: %s type: %s)", uid, caller, lock_str);
#endif
		goto free_caller;
	}

free_caller:
#if LOCK_DEBUG
	if (caller != NULL)
		free(caller);

	if (lock_str != NULL)
		free(lock_str);
#endif

	return rc;
}


inline int metadata_unlock(void)
{
#if LOCK_DEBUG
	char *caller = NULL;
	caller = get_caller();
	debug("unlocking metalock (caller: %s)",  caller);
	if (caller != NULL)
		free(caller);
#endif

	if (pthread_rwlock_unlock(&meta_lock) != 0) {
		debug("can't unlock metalock");
		return 0;
			
	}

	return 1;
}



void metadata_free_unused_tc_dir(void)
{
	tc_dir_meta_t *tc_dir = NULL;
	tc_dir_meta_t *tc_dir_next = NULL;

	for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir_next) {
		// in case we free tc_dir
		tc_dir_next = tc_dir->hh.next;

		if (metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY)) {
			if (tc_dir_trylock(tc_dir)) {
				if (tc_dir->refcount ==  0) { 
					debug("metadata_free_unused_tc_dir: refcount for %s is 0 -- freeing it", tc_dir->path);

					HASH_DEL(meta, tc_dir);

					// FIXME: can't unlock metalock here:
					// must wait for free_tc to finish 
					// new hdb might be opened before current closes (threading error)

					tc_dir_free(tc_dir); 
				} 
				else 
					tc_dir_unlock(tc_dir);
			}
			
			metadata_unlock();
		}

	}

}
