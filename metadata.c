#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include "uthash.h"
#include "metadata.h"
#include "utils.h"
#include "tc.h"


static tc_dir_meta_t *meta = NULL;
static pthread_rwlock_t meta_lock;
static pthread_mutex_t data_lock;

#ifdef TC_LOCK 
static pthread_rwlock_t tc_lock;
#endif

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


	tc_dir = open_tc(path, TC_UP_REFCOUNT);

	if (tc_dir->files == NULL) {
		if (create_file_hash(tc_dir) < 0) {
			fprintf(stderr, "can't create file hash\n");
			return NULL;
		}
	}



	return tc_dir;
}


tc_dir_meta_t *open_tc(const char *path, int up_refcount)
{
	tc_dir_meta_t *tc_dir = NULL;
	
	fprintf(stderr, "getting data_lock\n");
	if (pthread_mutex_lock(&data_lock) != 0) {
		fprintf(stderr, "can't get data_lock\n");
		return 0;
	}

	fprintf(stderr, "adding %s to metadata hash\n", path);

	if ((tc_dir = lookup_path(path)) != NULL) {

		pthread_mutex_lock(&tc_dir->lock);
		tc_dir->refcount += up_refcount;
		pthread_mutex_unlock(&tc_dir->lock);

		fprintf(stderr, "key %s already in metadata hash (refcount incs to %d)\n", path, tc_dir->refcount);
		goto release_lock;
	}

	fprintf(stderr, "looked it up, didn't find it (%s)\n", path);
	tc_dir = init_tc_dir(path, up_refcount);

	if (tc_dir == NULL)
		goto release_lock;

	if (!add_to_meta_hash(tc_dir)) {
		free_tc_dir(tc_dir);
		tc_dir = NULL;
	}

release_lock:
	fprintf(stderr, "releasing data_lock\n");
	pthread_mutex_unlock(&data_lock);
	return tc_dir;
}


int add_to_meta_hash(tc_dir_meta_t *tc_dir)
{
	if (pthread_rwlock_wrlock(&meta_lock) != 0) {
		fprintf(stderr, "can't get writelock\n");
		return 0;
	}

	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);

	pthread_rwlock_unlock(&meta_lock);

	return 1;
}

tc_dir_meta_t *init_tc_dir(const char *path, int refcount)
{
	TCHDB *hdb 				= NULL;
	tc_dir_meta_t *tc_dir	= NULL;
	char *tc_path			= NULL;
	int ecode;

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL)
		return NULL;

	tc_dir->path     = strdup(path);
	tc_dir->files    = NULL;
	tc_dir->hdb      = NULL; 
	tc_dir->refcount = refcount;

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

	if (!tchdbopen(hdb, tc_path, HDBOREADER | HDBONOLCK)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "add_path open error: %s\n", tchdberrmsg(ecode));
		goto hdb_error;
	}

	free(tc_path); // FIXME duplicated code... 
	tc_dir->hdb = hdb;

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
	tc_dir_meta_t *tc_dir;

	if (pthread_rwlock_rdlock(&meta_lock) != 0) {
		fprintf(stderr, "can't get read lock\n");
		return NULL;
	}
	HASH_FIND_STR(meta, path, tc_dir);
	pthread_rwlock_unlock(&meta_lock);

	if (tc_dir != NULL)
		fprintf(stderr, "found %s in hash\n", path);

	return tc_dir;
}

tc_file_meta_t *lookup_file(tc_dir_meta_t * tc_dir, const char *file)
{
	tc_file_meta_t *tc_file;
	HASH_FIND_STR(tc_dir->files, file, tc_file);
	return tc_file;
}


int tc_filesize(const char *path)
{
	int ecode, size;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);
	tc_dir_meta_t *tc_dir;

	fprintf(stderr, "fetching filesize for %s/%s\n", parent, leaf);

	tc_dir = open_tc(parent, TC_DONT_UP_REFCOUNT);
	free(parent);

	if (tc_dir == NULL)
		return -1;


	size = tchdbvsiz(tc_dir->hdb, leaf, strlen(leaf));

	if (size == -1) {
		ecode = tchdbecode(tc_dir->hdb);
		fprintf(stderr, "vsize error: %s\n", tchdberrmsg(ecode));
	}
	
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

	tc_dir = open_tc(parent, TC_UP_REFCOUNT);
	free(parent);

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

	fprintf(stderr, "getting data_lock\n");

	if (pthread_mutex_lock(&data_lock) != 0) {
		fprintf(stderr, "can't get data_lock\n");
		return 0;
	}

	tc_dir = lookup_path(path);

	if (tc_dir != NULL) {
		pthread_mutex_lock(&tc_dir->lock);
		tc_dir->refcount--;
		pthread_mutex_unlock(&tc_dir->lock);
		fprintf(stderr, "refcount for %s down to %d\n", path, tc_dir->refcount);
	}

	pthread_mutex_unlock(&data_lock);


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
	tc_file_meta_t *tc_file = NULL;
	int ecode;

	if (tc_dir != NULL) {
		if (tc_dir->path != NULL)
			free((char *)tc_dir->path);

		if (tc_dir->files != NULL) {
			for (tc_file = tc_dir->files; tc_file != NULL;
			     tc_file = tc_file->hh.next) {
				free((char *)tc_file->path);
				HASH_DEL(tc_dir->files, tc_file);
				free(tc_file);
			}
		}
		
		if (tc_dir->hdb != NULL) {
			if (!tchdbclose(tc_dir->hdb)) {
				ecode = tchdbecode(tc_dir->hdb);
				fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
			}
			tchdbdel(tc_dir->hdb);
		}


		pthread_mutex_destroy(&tc_dir->lock);

		free(tc_dir);
	}
}

void print_file_hash(tc_file_meta_t * tc_file)
{
	for (; tc_file != NULL; tc_file = tc_file->hh.next)
		fprintf(stderr, "path: %s (%d) size: %d\n", tc_file->path, strlen(tc_file->path), tc_file->size);

}


void tc_gc(void *data)
{
	tc_dir_meta_t *tc_dir;

	while (true) {
		fprintf(stderr, "ENTER THE COLLECTOR!!\n"); // always wanted to say that

		for (tc_dir = meta; tc_dir != NULL; tc_dir = tc_dir->hh.next) {
			if (tc_dir->refcount ==  0) { 
				fprintf(stderr, "refcount for %s is 0 -- freeing it\n", tc_dir->path);

				if (pthread_rwlock_wrlock(&meta_lock) != 0) {
					fprintf(stderr, "can't get writelock\n");
					return; // FIXME: actually, if we return, there no GC, fix this
				}

				HASH_DEL(meta, tc_dir);

				pthread_rwlock_unlock(&meta_lock);
				
				free_tc_dir(tc_dir);

			}

		}
		//pthread_testcancel();
		sleep(TC_GC_SLEEP);
	}

	fprintf(stderr, "gc loop exited\n");
}
