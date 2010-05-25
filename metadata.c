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
static pthread_mutex_t data_lock;
static pthread_mutex_t uid_lock;

#ifdef TC_LOCK 
static pthread_rwlock_t tc_lock;
#endif

static size_t _uid = 0;

int init_metadata(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock meta_lock\n");
		return -errno;
	}

	if (pthread_mutex_init(&data_lock, NULL) != 0) {
		fprintf(stderr, "can't init mutex data_lock\n");
		return -errno;
	}

	if (pthread_mutex_init(&uid_lock, NULL) != 0) {
		fprintf(stderr, "can't init mutex data_lock\n");
		return -errno;
	}


#ifdef TC_LOCK 
	fprintf(stderr, "TC_LOCK defined, will use rwlock for all tokyocabinet operations\n");

	if (pthread_rwlock_init(&tc_lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock\n");
		return -errno;
	}
#endif

	return 0;
}


tc_dir_meta_t *add_path(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;

	if (!datalock_lock())
		return NULL;
	
	tc_dir = open_tc(path);


	datalock_unlock();
/* 
	if (tc_dir->files == NULL) {
		if (create_file_hash(tc_dir) < 0) {
			fprintf(stderr, "can't create file hash\n");
			return NULL;
		}
	}
*/


	return tc_dir;
}


tc_dir_meta_t *open_tc(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;
	
	fprintf(stderr, "adding %s to metadata hash\n", path);

	if (!metalock_write_lock())
		return -1;

	if ((tc_dir = lookup_path(path)) != NULL) {
		fprintf(stderr, "key %s (%s) already in metadata hash (refcount incs to %d)\n", path, tc_dir->path,  tc_dir->refcount);

		goto return_tc;
	}


	fprintf(stderr, "looked it up, didn't find it (%s)\n", path);
	tc_dir = init_tc_dir(path);

	if (tc_dir == NULL)
		goto return_tc;

	
	if (!add_to_meta_hash(tc_dir)) {
		free_tc_dir(tc_dir);
		tc_dir = NULL;
	}
 
return_tc:
	metalock_unlock();

	return tc_dir; // tc_dir is locked
}


int add_to_meta_hash(tc_dir_meta_t *tc_dir)
{
	//if (!metalock_write_lock())
	//	return 0;

	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);

	//metalock_unlock();

	return 1;
}

tc_dir_meta_t *init_tc_dir(const char *path)
{
	TCHDB *hdb 				= NULL;
	tc_dir_meta_t *tc_dir	= NULL;
	char *tc_path			= NULL;
	int ecode;

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL)
		return NULL;


	tc_dir->path     = strdup(path);
	//tc_dir->files    = NULL;
	tc_dir->hdb      = NULL; 
	tc_dir->refcount = 1;

	if (pthread_mutex_init(&tc_dir->lock, NULL) != 0) {
		fprintf(stderr, "can't init mutex for tc_dir\n");
		goto free_tc_dir;
	}


	tc_path = to_tc_path(path);

	if (tc_path == NULL)
		goto free_tc_dir;

	hdb = tchdbnew();
	
	if (hdb == NULL)
		goto hdb_error;
	
	fprintf(stderr, "opening hdb (%s)\n", path);
	if (!tchdbopen(hdb, tc_path, HDBOREADER | HDBONOLCK)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
		goto hdb_error;
	}
	fprintf(stderr, "hdb opened (%s)\n", path);

	free(tc_path); // FIXME duplicated code... 
	tc_dir->hdb = hdb;

	if (!tc_dir_lock(tc_dir))
		goto free_tc_dir;

	return tc_dir;

hdb_error:
	if (tc_path != NULL)
		free(tc_path);
	
	if (hdb != NULL)
		tchdbdel(hdb);


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

/* 
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
*/

tc_dir_meta_t *lookup_path(const char *path)
{
	fprintf(stderr, "looking up %s\n", path);
	tc_dir_meta_t *tc_dir;

	//if (!metalock_read_lock())
	//	return NULL;

	HASH_FIND_STR(meta, path, tc_dir);

	if (tc_dir != NULL) {
		fprintf(stderr, "found %s in hash\n", path);
		tc_dir_lock(tc_dir);
		tc_dir->refcount++;
	}

	//metalock_unlock();

	return tc_dir; // returns tc_dir locked
}


int tc_filesize(const char *path)
{
	int ecode, size;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);
	tc_dir_meta_t *tc_dir;

	fprintf(stderr, "fetching filesize for %s/%s\n", parent, leaf);
	
	if (!datalock_lock())
		return -1;

	tc_dir = open_tc(parent);
	free(parent);

	if (tc_dir == NULL)
		return -1;

	size = tchdbvsiz(tc_dir->hdb, leaf, strlen(leaf));

	if (size == -1) {
		ecode = tchdbecode(tc_dir->hdb);
		fprintf(stderr, "vsize error: %s\n", tchdberrmsg(ecode));
	}

	tc_dir->refcount--;
	fprintf(stderr, "tc_filesize: refcount for %s down to %d\n", path, tc_dir->refcount);
	tc_dir_unlock(tc_dir);

	datalock_unlock();
	
	fprintf(stderr, "fetched filesize for %s is %d\n", leaf, size);
	return size;
}

char *tc_value(const char *path, int *value_len)
{
	tc_dir_meta_t *tc_dir;
	int ecode;
	char *value;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	fprintf(stderr, "fetching value for %s/%s\n", parent, leaf);

	if (!datalock_lock())
		return NULL;

	tc_dir = open_tc(parent);
	free(parent);

	/* 
	if (tc_dir == NULL)
		return NULL;
	*/

	value = tchdbget(tc_dir->hdb, leaf, strlen(leaf), value_len);

	if (value == NULL) {
		ecode = tchdbecode(tc_dir->hdb);
		fprintf(stderr, "getvalue error: %s\n", tchdberrmsg(ecode));
	}
	else {
		fprintf(stderr, "fetched %s size: %d\n", leaf, *value_len);
	}

	tc_dir_unlock(tc_dir);

	datalock_unlock();


	return value;
}





int release_path(const char *path)
{
	tc_dir_meta_t *tc_dir;

	if (!datalock_lock())
		return -1;

	if (!metalock_read_lock())
		return -1;

	tc_dir = lookup_path(path);

	if (tc_dir != NULL) {
		tc_dir->refcount -= 2; // extra dec for the lookup_path
		tc_dir_unlock(tc_dir);
		fprintf(stderr, "release_path: refcount for %s down to %d\n", path, tc_dir->refcount);
	}

	metalock_unlock();

	datalock_unlock();


	return 0;
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
		pthread_mutex_destroy(&tc_dir->lock);

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

		if (!datalock_lock())
			return;
		
		if (!metalock_write_lock())
			return; // FIXME: actually, if we return, there no GC, fix this

		for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir->hh.next) {
			int freed = 0;

			tc_dir_lock(tc_dir);

			if (tc_dir->refcount ==  0) { 
				fprintf(stderr, "refcount for %s is 0 -- freeing it\n", tc_dir->path);

				HASH_DEL(meta, tc_dir);
				
				free_tc_dir(tc_dir); // also unlocks
				freed = 1;
			}

			if (!freed)
				tc_dir_unlock(tc_dir);

		}

		metalock_unlock();
		datalock_unlock();

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


int datalock_lock(void)
{
	return 1;
	fprintf(stderr, "datalock lock\n");

	if (pthread_mutex_lock(&data_lock) != 0) {
		fprintf(stderr, "can't lock datalock\n");
		return 0;

	}

	return 1;
}

int datalock_unlock(void)
{
	return 1;
	fprintf(stderr, "datalock unlock\n");

	if (pthread_mutex_unlock(&data_lock) != 0) {
		fprintf(stderr, "can't unlock datalock\n");
		return 0;
			
	}
	return 1;
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

int tc_dir_lock(tc_dir_meta_t *tc_dir)
{
	size_t uid = unique_id();
	char *caller = get_caller();
	int rc = 0;

	if (tc_dir != NULL) {
		fprintf(stderr, "locking tc_dir %s (req: %u caller: %s)\n", tc_dir->path, uid, caller);
		if (pthread_mutex_lock(&tc_dir->lock) == 0) {
			fprintf(stderr, "locking tc_dir %s (res: %u caller: %s)\n", tc_dir->path, uid, caller);
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
	if (tc_dir != NULL) {
		fprintf(stderr, "unlocking tc_dir %s\n", tc_dir->path);
		return pthread_mutex_unlock(&tc_dir->lock);
	}
	return -1;
}


char *get_caller(void)
{
	void *array[10];
	size_t size;
	char **strings, *after_path = NULL, *caller = NULL;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);

	if (size > 2) {
		after_path = strchr(strings[2], '(');

		if (after_path != NULL)
			caller = strdup(after_path);
			
	}

	free (strings);

	return caller;
}
