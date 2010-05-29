#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#include "uthash.h"
#include "metadata.h"
#include "utils.h"
#include "tc.h"

#define LOCK_DEBUG 0
#define DEBUG 0

#if DEBUG
static inline void debug(const char *format, ...) 
{
	va_list va;

	va_start(va, format);

	vfprintf(stderr, format, va);

	va_end(va);
}
#else
static inline void debug(const char *format, ...) {}
#endif

static tc_dir_meta_t *meta = NULL;
static pthread_rwlock_t meta_lock;
static pthread_mutex_t uid_lock;
static pthread_mutex_t gc_mutex;
static pthread_cond_t gc_cond;

#if LOCK_DEBUG
static char *str_locktype[] = { "", "write", "read", "", "", "write + dont_unlock", "read + dont_unlock" };
#endif

static size_t _uid = 0;

static inline int add_to_meta_hash(tc_dir_meta_t *);
static inline int tc_dir_dec_refcount(tc_dir_meta_t *);
static inline int metalock_lock(TC_LOCKTYPE);
static inline int metalock_unlock(void);
static inline int tc_dir_lock(tc_dir_meta_t *);
static inline int tc_dir_unlock(tc_dir_meta_t *);
static inline size_t unique_id(void);
static int wake_up_gc(void);
static TC_RC lookup_path(const char *, tc_dir_meta_t **, TC_LOCKTYPE);
static tc_dir_meta_t *allocate_tc_dir(const char *);
static tc_dir_meta_t *init_tc_dir(tc_dir_meta_t *);
static void free_tc_dir(tc_dir_meta_t *);
static void free_tc_file(tc_file_meta_t *);


int init_metadata(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		debug("can't init rwlock meta_lock\n");
		return -errno;
	}

	if (pthread_mutex_init(&uid_lock, NULL) != 0) {
		debug("can't init mutex uid_lock\n");
		return -errno;
	}

	if (pthread_mutex_init(&gc_mutex, NULL) != 0) {
		debug("can't init mutex gc_mutex\n");
		return -errno;
	}

  	if (pthread_cond_init(&gc_cond, NULL) != 0) {
		debug("can't init condition gc_cond\n");
		return -errno;
	}

	return 0;
}


tc_dir_meta_t *open_tc(const char *path)
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

	if ((tc_dir = allocate_tc_dir(path)) == NULL)
		goto return_tc;
	
	if (!add_to_meta_hash(tc_dir)) 
		goto destroy_tc_dir;

	metalock_unlock();

	if (init_tc_dir(tc_dir))
		debug("returning new tc_dir %s (initial refcount %d)\n", path,  tc_dir->refcount);
	else
		debug("failed to initialize tc_dir %s\n", path);

	tc_dir_unlock(tc_dir);

	return tc_dir->initialized ? tc_dir : NULL;

destroy_tc_dir:
	free_tc_dir(tc_dir);
	tc_dir = NULL;

return_tc:
	metalock_unlock();

	return tc_dir; 
}


static inline int add_to_meta_hash(tc_dir_meta_t *tc_dir)
{
	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);

	return 1;
}

tc_dir_meta_t *init_tc_dir(tc_dir_meta_t *tc_dir) 
{
	size_t path_len = strlen(tc_dir->path);
	char tc_path[path_len + TC_PREFIX_LEN + 1];

	to_tc_path(tc_dir->path, tc_path);

	tc_dir->hdb = tchdbnew();
	
	if (tc_dir->hdb == NULL) {
		debug("failed to create a new hdb object\n");
		goto hdb_error;
	}
	
	
	debug("opening hdb (%s)\n", tc_dir->path);

	TC_RETRY_LOOP(tc_dir->hdb, tc_dir->path, tchdbopen(tc_dir->hdb, tc_path, HDBOREADER | HDBONOLCK), goto hdb_error);

	debug("hdb opened (%s)\n", tc_dir->path);

	// if all successful, raise refcount
	tc_dir->refcount = 1; 
	tc_dir->initialized = 1;

	return tc_dir;

hdb_error:
	if (tc_dir->hdb != NULL)
		tchdbdel(tc_dir->hdb);

	tc_dir->hdb = NULL;

	return NULL;
}

tc_dir_meta_t *allocate_tc_dir(const char *path)
{
	tc_dir_meta_t *tc_dir	= NULL;

	if (path == NULL) {
		debug("allocate_tc_dir got null path\n");
		return NULL;
	}

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL) {
		debug("can't allocate memory for tc_dir_meta_t struct\n");
		return NULL;
	}

	tc_dir->path 	 = strdup(path);
	tc_dir->files    = NULL;
	tc_dir->hdb      = NULL; 
	tc_dir->refcount = 0;
	tc_dir->initialized = 0;

	if (pthread_mutex_init(&tc_dir->lock, NULL) != 0) {
		debug("can't init rwlock for tc_dir\n");
		goto free_tc_dir;
	}

	if (!tc_dir_lock(tc_dir))
		goto free_tc_dir;

	return tc_dir;

free_tc_dir:
	free_tc_dir(tc_dir);
	return NULL;
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

	
	TC_RETRY_LOOP(tc_dir->hdb, tc_dir->path, ( 
				current_key = tchdbgetnext3(tc_dir->hdb, 
				last_key, last_key_len, 
				&current_key_len, &fetched_data, 
				&fetched_data_len)) != NULL, 
			break);

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
	char *leaf = leaf_file(path);
	tc_dir_meta_t *tc_dir;

	debug("fetching filesize for %s/%s\n", parent, leaf);
	
	tc_dir = open_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return -1;

	TC_RETRY_LOOP(tc_dir->hdb, tc_dir->path, (size = tchdbvsiz(tc_dir->hdb, leaf, strlen(leaf))) != -1, break);

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
	char *leaf = leaf_file(path);

	debug("fetching value for %s/%s\n", parent, leaf);

	tc_dir = open_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return 0;

	TC_RETRY_LOOP(tc_dir->hdb, tc_dir->path, ( value = tchdbget(tc_dir->hdb, leaf, strlen(leaf), &value_len) ) != NULL, break);

	if (value == NULL)  {
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

static inline int tc_dir_dec_refcount(tc_dir_meta_t *tc_dir)
{
	if (tc_dir == NULL) {
		debug("tried to refrence dec a null tc_dir\n");
		return 0;
	}

	if (!tc_dir_lock(tc_dir))
		return 0;
	
	tc_dir->refcount--;

	tc_dir_unlock(tc_dir);

	debug("refcount for %s decremented (refcount: %d)\n", tc_dir->path, tc_dir->refcount);
	return 1;
}

void free_tc_dir(tc_dir_meta_t * tc_dir)
{
	if (tc_dir != NULL) {
		debug("freeing tc_dir %s\n", tc_dir->path);

		if (tc_dir->hdb != NULL) {
			debug("closing hdb (%s)\n", tc_dir->path);

			TC_RETRY_LOOP(tc_dir->hdb, tc_dir->path, tchdbclose(tc_dir->hdb),break);

			tchdbdel(tc_dir->hdb);
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
		debug("asked to free a NULL tc_dir\n");
}

static int wake_up_gc(void)
{
	debug("STARVING MARVIN!!!\n");

	if (pthread_mutex_lock(&gc_mutex) != 0)
		return 0;

	if (pthread_cond_signal(&gc_cond) != 0)
		return 0;

	pthread_mutex_unlock(&gc_mutex);

	return 1;
}

static struct timespec *set_next_gc(struct timespec *ts, size_t seconds)
{
	struct timeval tp;

	if (gettimeofday(&tp, NULL) != 0)
		return 0;

	ts->tv_sec  = tp.tv_sec;
    ts->tv_nsec = tp.tv_usec * 1000;
    ts->tv_sec += seconds;

	return ts;
}

void tc_gc(void *data)
{
		tc_dir_meta_t *tc_dir = NULL;
		tc_dir_meta_t *tc_dir_next = NULL;
		struct timespec ts;

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

		while (true) {
			if (pthread_mutex_lock(&gc_mutex) != 0)
				continue;

			pthread_cond_timedwait(&gc_cond, &gc_mutex, set_next_gc(&ts, TC_GC_SLEEP));

			debug("tc_gc: ENTER THE COLLECTOR!!\n"); // always wanted to say that
			
			for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir_next) {
				// in case we free tc_dir
				tc_dir_next = tc_dir->hh.next;

				if (pthread_rwlock_trywrlock(&meta_lock) == 0) {
					if (pthread_mutex_trylock(&tc_dir->lock) == 0) {

						if (tc_dir->refcount ==  0) { 
							debug("tc_gc: refcount for %s is 0 -- freeing it\n", tc_dir->path);

							HASH_DEL(meta, tc_dir);

							// FIXME: can't unlock metalock here:
							// must wait for free_tc to finish 
							// new hdb might be opened before current closes (threading error)

							free_tc_dir(tc_dir); 
						} 
						else 
							tc_dir_unlock(tc_dir);
					}
					
					metalock_unlock();
				}

			}
			pthread_mutex_unlock(&gc_mutex);
			pthread_testcancel();
		}

		debug("gc loop exited\n");
}

static inline size_t unique_id(void)
{
	size_t uid;

	pthread_mutex_lock(&uid_lock);
	uid = _uid++;
	pthread_mutex_unlock(&uid_lock);

	return uid;
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

static inline int tc_dir_lock(tc_dir_meta_t *tc_dir)
{
#if LOCK_DEBUG
	size_t uid = unique_id();
	char *caller = get_caller();
#endif
	int rc = 0;

	if (tc_dir != NULL) {
#if LOCK_DEBUG
		debug("locking tc_dir %s (req: %u caller: %s)\n", tc_dir->path, uid, caller);
#endif

		if (pthread_mutex_lock(&tc_dir->lock) == 0) {
#if LOCK_DEBUG
			debug("locking tc_dir %s (res: %u caller: %s)\n", tc_dir->path, uid, caller); 
#endif
			rc = 1;
			goto free_caller;
		}
		else {
			debug("unable to lock tc_dir\n");
			goto free_caller;
		}
	}
	debug("unable to lock tc_dir - it's null\n");

free_caller:
#if LOCK_DEBUG
	if (caller != NULL)
		free(caller);
#endif 

	return rc;
}

static inline int tc_dir_unlock(tc_dir_meta_t *tc_dir)
{

#if LOCK_DEBUG
	char *caller = NULL;
#endif

	if (tc_dir != NULL) {
#if LOCK_DEBUG
		caller = get_caller();
		debug("unlocking tc_dir %s (caller: %s)\n", tc_dir->path, caller);
#endif
		pthread_mutex_unlock(&tc_dir->lock);
#if LOCK_DEBUG
		if (caller != NULL)
			free(caller);
#endif
		return 1;
	}
	else 
		debug("tried tc_dir_unlock on null tc_dir\n");

	return 0;
}



