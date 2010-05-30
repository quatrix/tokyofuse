#include <tchdb.h>
#include <unistd.h>
#include <string.h>
#include "tc_gc.h"
#include "utils.h"

#define TC_CABINET_TRIES 500 // retries for tchdb* functions
#define TC_CABINET_USLEEP 30 // micro seconds

#define TC_RETRY_LOOP(hdb, path, cond, on_failure) 																\
do {																											\
	int i = 0, ecode = 0;																						\
																												\
	while (!(cond)) {																							\
		ecode = tchdbecode((hdb));																				\
																												\
		debug("%s error: %s (%s) [%d/%d]\n", #cond, tchdberrmsg(ecode), (path), i, TC_CABINET_TRIES);	\
																												\
		if (ecode != TCEMMAP || ++i == TC_CABINET_TRIES) {														\
			on_failure;																							\
			break;																								\
		}																										\
																												\
		wake_up_gc();																							\
		usleep(TC_CABINET_USLEEP);																				\
	}																											\
																												\
} while (0)


TCHDB *open_tc(const char *path) {
	TCHDB *hdb = NULL;

	if ((hdb = tchdbnew()) == NULL) {
		debug("failed to create a new hdb object\n");
		return NULL;
	}

	debug("opening hdb (%s)\n", path);

	TC_RETRY_LOOP(hdb, path, tchdbopen(hdb, path, HDBOREADER | HDBONOLCK), goto hdb_error);

	debug("hdb opened (%s)\n", path);

	return hdb;

hdb_error:
	if (hdb != NULL)
		tchdbdel(hdb);

	return NULL;
}


char *tc_get_next(TCHDB *hdb, const char *path, char *last_key, 
					int last_key_len, int *current_key_len, 
					const char **fetched_data, int *fetched_data_len)
{
	char *current_key = NULL;

	TC_RETRY_LOOP(hdb, path, ( 
					current_key = tchdbgetnext3(hdb, 
					last_key, last_key_len, 
					current_key_len, fetched_data, 
					fetched_data_len)) != NULL, 
			break);

	return current_key;
}

int tc_get_filesize(TCHDB *hdb, const char *path, const char *key)
{
	size_t key_len = strlen(key);
	int size;

	TC_RETRY_LOOP(hdb, path, (size = tchdbvsiz(hdb, key, key_len)) != -1, break);

	return size;
}

char *tc_get_value(TCHDB *hdb, const char *path,  const char *key, int *value_len)
{
	size_t key_len = strlen(key);
	char *value;

	TC_RETRY_LOOP(hdb, path, ( value = tchdbget(hdb, key, key_len, value_len) ) != NULL, break);

	return value;
}

int tc_close(TCHDB *hdb, const char *path)
{
	int rc = 0;
	TC_RETRY_LOOP(hdb, path, tchdbclose(hdb),goto error);
	rc = 1;

error:
	return rc;
}





