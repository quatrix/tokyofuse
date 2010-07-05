#ifndef METADATA_H
#define METADATA_H
#include <tchdb.h>
#include "tc_dir.h"


struct tc_file_meta {
	char *path;
	size_t path_len;
	size_t size;
};

typedef struct tc_file_meta tc_file_meta_t;


struct tc_filehandle {
	char *value;
	int value_len;
	tc_dir_meta_t *tc_dir;
};

typedef struct tc_filehandle tc_filehandle_t;


typedef enum {
	TC_LOCK_READ		= 0,
	TC_LOCK_WRITE  		= 1,
	TC_LOCK_DONT_UNLOCK = 2, 
	TC_LOCK_TRY 		= 4
} TC_LOCKTYPE;

#define TC_LOCK_ALL "TC_LOCK_READ TC_LOCK_WRITE TC_LOCK_DONT_UNLOCK TC_LOCK_TRY"

typedef enum {
	TC_ERROR,
	TC_EXISTS,
	TC_NOT_FOUND
} TC_RC;

int metadata_init(void);

tc_dir_meta_t *metadata_get_tc(const char *, size_t);
int metadata_release_path(tc_dir_meta_t *);
int metadata_get_filesize(const char *, size_t , char *, size_t );
int metadata_get_value(const char *, size_t , tc_filehandle_t *, char *, size_t );

void metadata_free_unused_tc_dir(void);

tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *, tc_file_meta_t *);

int metadata_add_to_hash(tc_dir_meta_t *);
inline int metadata_lock(TC_LOCKTYPE);
inline int metadata_unlock(void);
TC_RC metadata_lookup_path(const char *, size_t, tc_dir_meta_t **, TC_LOCKTYPE);
void free_tc_file(tc_file_meta_t *);


#endif
