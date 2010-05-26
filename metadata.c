#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <execinfo.h>
#include "uthash.h"
#include "metadata.h"
#include "utils.h"
#include "tc.h"


static tc_dir_meta_t *meta = NULL;
static pthread_rwlock_t meta_lock;
static pthread_mutex_t uid_lock;

static char *str_locktype[] = { "write", "read" };
static size_t _uid = 0;

int init_metadata(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock meta_lock\n");
		return -errno;
	}

	if (pthread_mutex_init(&uid_lock, NULL) != 0) {
		fprintf(stderr, "can't init mutex uid_lock\n");
		return -errno;
	}


	return 0;
}


tc_dir_meta_t *add_path(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;

	tc_dir = open_tc(path);
/* 
	if (tc_dir != NULL) {
		tc_dir_lock(tc_dir, TC_LOCK_WRITE);

		if (tc_dir->files == NULL) {
			if (create_file_hash(tc_dir) < 0) {
				fprintf(stderr, "can't create file hash\n");
				tc_dir_dec_refcount(tc_dir, 1);
				tc_dir_unlock(tc_dir);
				return NULL;
			}
		}

		tc_dir_unlock(tc_dir);
	}
*/

	return tc_dir;
}

tc_dir_meta_t *open_tc(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;
	
	fprintf(stderr, "adding %s to metadata hash\n", path);

	if (!metalock_read_lock())
		return NULL;

	if ((tc_dir = lookup_path(path)) != NULL) {
		fprintf(stderr, "key %s already in metadata hash (refcount incs to %d)\n", path, tc_dir->refcount);

		goto return_tc; 
	}

	metalock_unlock();

	// if key doesn't exist, lock for write

	if (!metalock_write_lock())
		return NULL;

	// check again, since read locks allow multiple readers, 
	// so multiple threads could be waiting for this write lock
	// and one of them could already have created the key
	
	if ((tc_dir = lookup_path(path)) != NULL) {
		fprintf(stderr, "other writer already created key %s (refcount incs to %d)\n", path,  tc_dir->refcount);

		goto return_tc;
	}

	fprintf(stderr, "looked it up, didn't find it (%s)\n", path);
	tc_dir = allocate_tc_dir(path);

	if (tc_dir == NULL)
		goto return_tc;

	
	if (!add_to_meta_hash(tc_dir)) {
		free_tc_dir(tc_dir);
		tc_dir = NULL;
		goto return_tc;
	}

	metalock_unlock();

	if (!init_tc_dir(tc_dir)) {
		fprintf(stderr, "failed to initialize tc_dir %s\n", path);
		tc_dir_unlock(tc_dir);
		return NULL;
	}

	tc_dir_unlock(tc_dir);
	return tc_dir;

return_tc:
	metalock_unlock();

	return tc_dir; 
}


int add_to_meta_hash(tc_dir_meta_t *tc_dir)
{
	//if (!metalock_write_lock())
	//	return 0;

	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);

	//metalock_unlock();

	return 1;
}

tc_dir_meta_t *init_tc_dir(tc_dir_meta_t *tc_dir) 
{
	TCHDB *hdb 				= NULL;
	char *tc_path			= NULL;
	int ecode;

	tc_path = to_tc_path(tc_dir->path);

	hdb = tchdbnew();
	
	if (hdb == NULL) {
		fprintf(stderr, "failed to create a new hdb object\n");
		goto hdb_error;
	}
	
	fprintf(stderr, "opening hdb (%s)\n", tc_dir->path);
	if (!tchdbopen(hdb, tc_path, HDBOREADER | HDBONOLCK)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
		goto hdb_error;
	}
	fprintf(stderr, "hdb opened (%s)\n", tc_dir->path);

	free(tc_path); // FIXME duplicated code... 
	tc_dir->hdb = hdb;

	// if all successful, raise refcount
	tc_dir->refcount = 1; 
	tc_dir->initialized = 1;

	return tc_dir;

hdb_error:
	if (tc_path != NULL)
		free(tc_path);
	
	if (hdb != NULL)
		tchdbdel(hdb);

	tc_dir->hdb = NULL;

	return NULL;
}

tc_dir_meta_t *allocate_tc_dir(const char *path)
{
	tc_dir_meta_t *tc_dir	= NULL;

	if (path == NULL) {
		fprintf(stderr, "allocate_tc_dir got null path\n");
		return NULL;
	}

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL) {
		fprintf(stderr, "can't allocate memory for tc_dir_meta_t struct\n");
		return NULL;
	}

	tc_dir->path 	 = strdup(path);
	tc_dir->files    = NULL;
	tc_dir->hdb      = NULL; 
	tc_dir->refcount = 0;
	tc_dir->initialized = 0;

	if (pthread_rwlock_init(&tc_dir->lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock for tc_dir\n");
		goto free_tc_dir;
	}

	if (!tc_dir_lock(tc_dir, TC_LOCK_WRITE))
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

	//fprintf(stderr, "last_key: %s last_key_len: %d\n", last_key, last_key_len);

	current_key = tchdbgetnext3(tc_dir->hdb, last_key,
					last_key_len, &current_key_len,
					&fetched_data, &fetched_data_len);

	free_tc_file(last_tc_file);

	if (current_key != NULL) {
		tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));

		if (tc_file == NULL) {
			fprintf(stderr, "can't allocate memory for new tc_file\n");
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

int create_file_hash(tc_dir_meta_t *tc_dir)
{
	tc_file_meta_t *tc_file = NULL;

	const char *fetched_data;
	char *current_key = NULL;
	char *last_key = NULL;

	int fetched_data_len = 0;
	int current_key_len = 0;
	int last_key_len = 0;

	while (true) {
		last_key = current_key;
		last_key_len = current_key_len;

		current_key = tchdbgetnext3(tc_dir->hdb, last_key,
					    last_key_len, &current_key_len,
					    &fetched_data, &fetched_data_len);

		free(last_key);

		if (current_key != NULL) {
			tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));

			if (tc_file == NULL) {
				fprintf(stderr,
					"can't allocate memory for new tc_file\n");
				return -errno;
			}

			tc_file->path = strndup(current_key, current_key_len);
			tc_file->size = fetched_data_len;
			
			//fprintf(stderr, "key: %s ksiz: %d val_len: %d\n", tc_file->path, current_key_len,  fetched_data_len);

			HASH_ADD_KEYPTR(hh, tc_dir->files, tc_file->path, current_key_len, tc_file);
		}
		else 
			break;
	}




	return 0;
}

tc_dir_meta_t *lookup_path(const char *path)
{
	fprintf(stderr, "looking up %s\n", path);
	tc_dir_meta_t *tc_dir = NULL;
	tc_dir_meta_t *tc_dir_rc = NULL;


	HASH_FIND_STR(meta, path, tc_dir);

	if (tc_dir != NULL) {
		fprintf(stderr, "found %s in hash\n", path);
		tc_dir_lock(tc_dir, TC_LOCK_WRITE);

		if (tc_dir->initialized) {
			tc_dir->refcount++;
			tc_dir_rc = tc_dir;
		}
		else {
			fprintf(stderr, "found %s in hash - but it's not initialized\n", path);
		}

		tc_dir_unlock(tc_dir);
	}


	return tc_dir_rc;
}


int tc_filesize(const char *path)
{
	int ecode, size;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);
	tc_dir_meta_t *tc_dir;

	fprintf(stderr, "fetching filesize for %s/%s\n", parent, leaf);
	
	tc_dir = open_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return -1;

	//if (tc_dir->files == NULL) 
		size = tchdbvsiz(tc_dir->hdb, leaf, strlen(leaf));
	//else
	//	size = tc_dir_get_filesize(tc_dir, leaf);

	if (size == -1) {
		ecode = tchdbecode(tc_dir->hdb);
		fprintf(stderr, "vsize error: %s\n", tchdberrmsg(ecode));
	}

	if (!tc_dir_dec_refcount(tc_dir, 1))
		return -1;

	
	fprintf(stderr, "fetched filesize for %s is %d\n", leaf, size);
	return size;
}

int tc_dir_get_filesize(tc_dir_meta_t *tc_dir, const char *path)
{

	return -1;
}

char *tc_value(const char *path, int *value_len)
{
	tc_dir_meta_t *tc_dir;
	int ecode;
	char *value;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	fprintf(stderr, "fetching value for %s/%s\n", parent, leaf);

	tc_dir = open_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return NULL;

	value = tchdbget(tc_dir->hdb, leaf, strlen(leaf), value_len);

	if (value == NULL) {
		ecode = tchdbecode(tc_dir->hdb);
		fprintf(stderr, "getvalue error: %s\n", tchdberrmsg(ecode));
	}
	else {
		fprintf(stderr, "fetched %s size: %d\n", leaf, *value_len);
	}


	return value;
}





int release_path(const char *path)
{
	tc_dir_meta_t *tc_dir;
	int rc = 0;

	if (!metalock_read_lock())
		return 0;

	tc_dir = lookup_path(path);

	if (tc_dir != NULL)
		if (tc_dir_dec_refcount(tc_dir, 2))
			rc = 1;

	metalock_unlock();

	return rc;
}

int tc_dir_dec_refcount(tc_dir_meta_t *tc_dir, int ref)
{
	if (tc_dir == NULL) {
		fprintf(stderr, "tried to refrence dec a null tc_dir\n");
		return 0;
	}

	fprintf(stderr, "trying to dec refcount for %s\n", tc_dir->path);

	if (!tc_dir_lock(tc_dir, TC_LOCK_WRITE))
		return 0;
	
	tc_dir->refcount -= ref;

	tc_dir_unlock(tc_dir);

	fprintf(stderr, "refcount for %s decremented (refcount: %d)\n", tc_dir->path, tc_dir->refcount);
	return 1;
}

int release_file(const char *path)
{
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	fprintf(stderr, "releasing %s/%s\n", parent, leaf);

	release_path(parent);
	free(parent);


	return 0;
}


void free_tc_dir(tc_dir_meta_t * tc_dir)
{

	//tc_file_meta_t *tc_file = NULL;
	int ecode;

	if (tc_dir != NULL) {
		fprintf(stderr, "freeing tc_dir %s\n", tc_dir->path);


/* 
		if (tc_dir->files != NULL) {
			for (tc_file = tc_dir->files; tc_file != NULL;
			     tc_file = tc_file->hh.next) {
				free((char *)tc_file->path);
				HASH_DEL(tc_dir->files, tc_file);
				free(tc_file);
			}
		}
*/
		
		if (tc_dir->hdb != NULL) {
			fprintf(stderr, "closing hdb (%s)\n", tc_dir->path);
			if (!tchdbclose(tc_dir->hdb)) {
				ecode = tchdbecode(tc_dir->hdb);
				fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
			}
			fprintf(stderr, "hdb closed (%s)\n", tc_dir->path);
			tchdbdel(tc_dir->hdb);
			tc_dir->hdb = NULL;
			fprintf(stderr, "hdb fh deleted (%s)\n", tc_dir->path);
		}

		if (tc_dir->path != NULL) {
			free((char *)tc_dir->path);
			tc_dir->path = NULL;
		}


		tc_dir_unlock(tc_dir);
		pthread_rwlock_destroy(&tc_dir->lock);

		free(tc_dir);
	}
	else
		fprintf(stderr, "asked to free a NULL tc_dir\n");
}

/* 
void print_file_hash(tc_file_meta_t * tc_file)
{
	for (; tc_file != NULL; tc_file = tc_file->hh.next)
		fprintf(stderr, "path: %s (%d) size: %d\n", tc_file->path, strlen(tc_file->path), tc_file->size);

}
*/

void tc_gc(void *data)
{
	tc_dir_meta_t *tc_dir;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	while (true) {
		fprintf(stderr, "ENTER THE COLLECTOR!!\n"); // always wanted to say that



		for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir->hh.next) {
			int freed = 0;

			if (!metalock_write_lock())
				return; // FIXME: actually, if we return, there no GC, fix this

			tc_dir_lock(tc_dir, TC_LOCK_WRITE);

			if (tc_dir->refcount ==  0) { 
				fprintf(stderr, "refcount for %s is 0 -- freeing it\n", tc_dir->path);


				HASH_DEL(meta, tc_dir);
			

				
				free_tc_dir(tc_dir); // also unlocks
				
				freed = 1;
			}

			if (!freed)
				tc_dir_unlock(tc_dir);
			
			metalock_unlock();

		}



		pthread_testcancel();
		sleep(TC_GC_SLEEP);
	}

	fprintf(stderr, "gc loop exited\n");
}

size_t unique_id(void)
{
	size_t uid;

	pthread_mutex_lock(&uid_lock);
	uid = _uid++;
	pthread_mutex_unlock(&uid_lock);

	return uid;
}



int metalock_write_lock(void)
{
	size_t uid = unique_id();
	char *caller = get_caller();
	int rc = 1;

	fprintf(stderr, "metalock write lock (req: %u caller: %s)\n", uid, caller);

	if (pthread_rwlock_wrlock(&meta_lock) != 0) {
		//fprintf(stderr, "can't lock write metalock\n");
		perror("can't lock write metalock");
		rc = 0;
		goto free_caller;

	}
	fprintf(stderr, "metalock write lock (res: %u caller: %s)\n", uid, caller);

free_caller:
	if (caller != NULL)
		free(caller);

	return rc;
}

int metalock_read_lock(void)
{
	size_t uid = unique_id();
	char *caller = get_caller();
	int rc = 1;

	fprintf(stderr, "metalock read lock (req: %u caller: %s)\n", uid, caller);

	if (pthread_rwlock_rdlock(&meta_lock) != 0) {
		//fprintf(stderr, "can't lock read metalock\n");
		perror("can't lock read metalock");
		rc = 0;
		goto free_caller;
	}
	fprintf(stderr, "metalock read lock (res: %u caller: %s)\n", uid, caller);

free_caller:
	if (caller != NULL)
		free(caller);

	return rc;
}

int metalock_unlock(void)
{
	fprintf(stderr, "metalock unlock\n");

	if (pthread_rwlock_unlock(&meta_lock) != 0) {
		fprintf(stderr, "can't unlock metalock\n");
		return 0;
			
	}
	return 1;
}

int tc_dir_lock(tc_dir_meta_t *tc_dir, TC_LOCKTYPE locktype)
{
	size_t uid = unique_id();
	char *caller = get_caller();
	int l_rc = 0;
	int rc = 0;

	if (tc_dir != NULL) {
		fprintf(stderr, "locking tc_dir %s (req: %u caller: %s type: %s)\n", tc_dir->path, uid, caller, str_locktype[locktype]);

		if (locktype == TC_LOCK_WRITE) 
			l_rc = pthread_rwlock_wrlock(&tc_dir->lock);
		else if (locktype == TC_LOCK_READ)
			l_rc = pthread_rwlock_rdlock(&tc_dir->lock);
		else {
			fprintf(stderr, "unknown lock type specified\n");
			goto free_caller;
		}

		if (l_rc == 0) {
			fprintf(stderr, "locking tc_dir %s (res: %u caller: %s type: %s)\n", tc_dir->path, uid, caller, str_locktype[locktype]);
			rc = 1;
			goto free_caller;
		}
		else {
			fprintf(stderr, "unable to lock tc_dir\n");
			goto free_caller;
		}
	}
	fprintf(stderr, "unable to lock tc_dir - it's null\n");

free_caller:
	if (caller != NULL)
		free(caller);

	return rc;
}

int tc_dir_unlock(tc_dir_meta_t *tc_dir)
{

	char *caller = NULL;

	if (tc_dir != NULL) {
		caller = get_caller();
		fprintf(stderr, "unlocking tc_dir %s (caller: %s)\n", tc_dir->path, caller);
		pthread_rwlock_unlock(&tc_dir->lock);
		free(caller);
		return 1;
	}
	else {
		fprintf(stderr, "tried tc_dir_unlock on null tc_dir\n");
	}

	return 0;
}


char *get_caller(void)
{
	void *array[10];
	size_t size;
	char **strings, *after_path = NULL, *caller = NULL;

	size = backtrace(array, 10);
	strings = backtrace_symbols(array, size);

	if (size > 2) {
		after_path = strchr(strings[2], '(');

		if (after_path != NULL)
			caller = strdup(after_path);
			
	}

	free (strings);

	return caller;
}
