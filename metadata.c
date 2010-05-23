#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "uthash.h"
#include "metadata.h"
#include "utils.h"
#include "tc.h"

tc_dir_meta_t *meta = NULL;
pthread_rwlock_t meta_lock;
pthread_rwlock_t tc_lock;

int uid = 0;

int init_metadata(void)
{
	if (pthread_rwlock_init(&meta_lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock\n");
		return -errno;
	}

	if (pthread_rwlock_init(&tc_lock, NULL) != 0) {
		fprintf(stderr, "can't init rwlock\n");
		return -errno;
	}

	return 0;
}

tc_dir_meta_t *add_path(const char *path)
{
	TCHDB *hdb;
	tc_dir_meta_t *tc_dir = NULL;
	tc_file_meta_t *tc_file = NULL;
	int ecode;
	

	fprintf(stderr, "adding %s to metadata hash\n", path);

	if (pthread_rwlock_wrlock(&tc_lock) != 0) {
		fprintf(stderr, "can't get writelock\n");
		return NULL;
	}

	if ((tc_dir = lookup_path(path)) != NULL) {
		fprintf(stderr, "key %s already in metadata hash\n", path);
		pthread_rwlock_unlock(&tc_lock);
		return tc_dir;
	}

	fprintf(stderr, "looked it up, didn't find it (%s)\n", path);

	tc_dir = (tc_dir_meta_t *) malloc(sizeof(tc_dir_meta_t));

	if (tc_dir == NULL) {
		pthread_rwlock_unlock(&tc_lock);
		return NULL;
	}

	tc_dir->path = strdup(path);
	tc_dir->files = NULL;
	tc_dir->uid = uid++;

	hdb = tchdbnew();

	char *tc_path = to_tc_path(path);

	if (!tchdbopen(hdb, tc_path, HDBOREADER)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "add_path open error: %s\n",
			tchdberrmsg(ecode));
		free(tc_path);
		pthread_rwlock_unlock(&tc_lock);
		return NULL;
	}

	free(tc_path);
/* 
	if (!tchdbforeach(hdb, tc_file_cb, tc_dir)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "foreach error: %s\n", tchdberrmsg(ecode));
		pthread_rwlock_unlock(&tc_lock);
		return NULL;
	}
*/

	const char *fetched_data;
	char *current_key = NULL;
	char *last_key = NULL;

	int fetched_data_len = 0;
	int current_key_len = 0;
	int last_key_len = 0;
/* 
	tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));
	char *b = strdup("hey");

	tc_file->path = b;
	tc_file->size  = 5;
	HASH_ADD_KEYPTR(hh, tc_dir->files, b, 3, tc_file);
	*/

	while (true) {
		last_key = current_key;
		last_key_len = current_key_len;

		current_key = tchdbgetnext3(hdb, last_key,
					    last_key_len, &current_key_len,
					    &fetched_data, &fetched_data_len);

		free(last_key);

		if (current_key != NULL) {
			tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));

			if (tc_file == NULL) {
				fprintf(stderr,
					"can't allocate memory for new tc_file\n");
				return NULL;
			}

			tc_file->path = strndup(current_key, current_key_len);
			tc_file->size = fetched_data_len;
			
			fprintf(stderr, "key: %s ksiz: %d val_len: %d\n", tc_file->path, last_key_len,  fetched_data_len);

			HASH_ADD_KEYPTR(hh, tc_dir->files, tc_file->path, last_key_len, tc_file);
		}
		else 
			break;
	}

	print_file_hash(tc_dir->files);

	if (!tchdbclose(hdb)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
	}

	tchdbdel(hdb);

	if (pthread_rwlock_wrlock(&meta_lock) != 0) {
		fprintf(stderr, "can't get writelock\n");
		pthread_rwlock_unlock(&tc_lock);
		return NULL;
	}

	HASH_ADD_KEYPTR(hh, meta, tc_dir->path, strlen(tc_dir->path), tc_dir);
	pthread_rwlock_unlock(&meta_lock);
	pthread_rwlock_unlock(&tc_lock);

	return tc_dir;
}

bool tc_file_cb(const void *key, int ksiz, const void *vbuf, int vsiz,
		void *tc_dm)
{

	tc_file_meta_t *tc_file = NULL;
	tc_dir_meta_t *tc_dir = (tc_dir_meta_t *) tc_dm;

	tc_file = (tc_file_meta_t *) malloc(sizeof(tc_file_meta_t));

	if (tc_file == NULL) {
		fprintf(stderr, "can't allocate memory for new tc_file\n");
		return false;
	}

	tc_file->path = strndup(key, ksiz);
	tc_file->size = vsiz;

	//fprintf(stderr, "IN CALLBACK %s (ksiz: %d) %d\n", tc_file->path, ksiz, tc_file->size);

	HASH_ADD_KEYPTR(hh, tc_dir->files, tc_file->path, ksiz, tc_file);

	return true;
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

int meta_filesize(const char *path)
{
	tc_dir_meta_t *tc_dir = NULL;
	tc_file_meta_t *tc_file = NULL;

	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	//fprintf(stderr, "parent: %s leaf: %s\n", parent, leaf);

	tc_dir = lookup_path(parent);
	free(parent);

	if (tc_dir == NULL)
		return tc_filesize(path);

	tc_file = lookup_file(tc_dir, leaf);

	if (tc_file == NULL) {
		fprintf(stderr, "can't find meta file %s\n", leaf);
		return -1;
	}

	return tc_file->size;
}

int tc_filesize(const char *path)
{
	TCHDB *hdb;
	int ecode, size;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	fprintf(stderr, "fetching filesize for %s/%s\n", parent, leaf);

	hdb = tchdbnew();

	char *tc_path = to_tc_path(parent);

	if (tc_path == NULL)
		return -errno;

	if (pthread_rwlock_wrlock(&tc_lock) != 0) {
		fprintf(stderr, "can't get writelock\n");
		return -errno;
	}

	if (!tchdbopen(hdb, tc_path, HDBOREADER)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "tc_filesize: open error: %s\n",
			tchdberrmsg(ecode));
		free(tc_path);
		free(parent);
		pthread_rwlock_unlock(&tc_lock);
		return -errno;
	}

	free(tc_path);
	free(parent);

	size = tchdbvsiz(hdb, leaf, strlen(leaf));

	if (size == -1) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "vsize error: %s\n", tchdberrmsg(ecode));
	}

	if (!tchdbclose(hdb)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
	}

	tchdbdel(hdb);
	pthread_rwlock_unlock(&tc_lock);
	
	fprintf(stderr, "fetched filesize for %s is %d\n", leaf, size);
	return size;
}

char *tc_value(const char *path, int *value_len)
{
	TCHDB *hdb;
	int ecode;
	char *value;
	char *parent = parent_path(path);
	char *leaf = leaf_file(path);

	fprintf(stderr, "fetching value for %s/%s\n", parent, leaf);

	hdb = tchdbnew();

	char *tc_path = to_tc_path(parent);

	if (tc_path == NULL)
		return NULL;

	if (pthread_rwlock_wrlock(&tc_lock) != 0) {
		fprintf(stderr, "can't get writelock\n");
		return NULL;
	}

	if (!tchdbopen(hdb, tc_path, HDBOREADER)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "tc_value open error: %s\n",
			tchdberrmsg(ecode));
		free(tc_path);
		free(parent);
		pthread_rwlock_unlock(&tc_lock);
		return NULL;
	}

	free(tc_path);
	free(parent);

	value = tchdbget(hdb, leaf, strlen(leaf), value_len);

	if (value == NULL) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "getvalue error: %s\n", tchdberrmsg(ecode));
	}
	else {
		fprintf(stderr, "fetched %s size: %d\n", leaf, *value_len);
	}

	if (!tchdbclose(hdb)) {
		ecode = tchdbecode(hdb);
		fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
	}

	tchdbdel(hdb);
	pthread_rwlock_unlock(&tc_lock);


	return value;
}

int remove_path(const char *path)
{
	tc_dir_meta_t *tc_dir;

	tc_dir = lookup_path(path);

	if (tc_dir != NULL) {
		fprintf(stderr, "removing path %s\n", path);

		if (pthread_rwlock_wrlock(&meta_lock) != 0) {
			fprintf(stderr, "can't get writelock\n");
			return -errno;
		}

		HASH_DEL(meta, tc_dir);
		free_tc_dir(tc_dir);

		pthread_rwlock_unlock(&meta_lock);
	}

	return 0;
}

void free_tc_dir(tc_dir_meta_t * tc_dir)
{
	tc_file_meta_t *tc_file = NULL;

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

		free(tc_dir);
	}
}

void print_file_hash(tc_file_meta_t * tc_file)
{
	for (; tc_file != NULL; tc_file = tc_file->hh.next)
		fprintf(stderr, "path: %s (%d) size: %d\n", tc_file->path,
			strlen(tc_file->path), tc_file->size);

}
