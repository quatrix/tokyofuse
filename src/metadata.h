#ifndef METADATA_H
#define METADATA_H
#include <tchdb.h>
#include "uthash.h"
#include "tc_dir.h"


struct tc_file_meta {
	char *path;
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
	TC_LOCK_WRITE  		= 1,
	TC_LOCK_READ		= 2,
	TC_LOCK_DONT_UNLOCK = 4
} TC_LOCKTYPE;

typedef enum {
	TC_ERROR,
	TC_EXISTS,
	TC_NOT_FOUND
} TC_RC;

int init_metadata(void);

tc_dir_meta_t *get_tc(const char *);
int release_path(tc_dir_meta_t *);
int tc_filesize(const char *);
int tc_value(const char *, tc_filehandle_t *);

void remove_unused_tc_dir(void);

tc_file_meta_t *get_next_tc_file(tc_dir_meta_t *, tc_file_meta_t *);



#endif
