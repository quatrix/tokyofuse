#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tc.h"
#include "utils.h"



char *to_tc_path(const char *path, char *dst_path)
{
	if (path == NULL)
		return NULL;
	
	sprintf(dst_path, "%s.tc", path);
	return dst_path;
}

void tc_dir_stat(struct stat *stbuf)
{
	// faking a tc file to look like a directory

	memset(stbuf, 0, sizeof(stbuf));

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = time(NULL);
	stbuf->st_mode = S_IFDIR | 0555;
	stbuf->st_nlink = 2;
}

void tc_file_stat(struct stat *stbuf, size_t size)
{
	// faking a tc key to look like a file

	memset(stbuf, 0, sizeof(stbuf));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = time(NULL);

	stbuf->st_mode = S_IFREG | 0444;
	stbuf->st_nlink = 1;
	stbuf->st_size = size;
}

int is_tc(const char *path) 
{
	if (path == NULL)
		return 0;

	size_t path_len = strlen(path);
	char tc_path[path_len + TC_PREFIX_LEN + 1];

	if (to_tc_path(path, tc_path) == NULL)
		return 0;

	if (file_exists(tc_path))
		return 1;
		
	return 0;
}

int is_parent_tc(const char *path)
{
	int rc = 0;
	
	if (path == NULL)
		return 0;

	char *parent = parent_path(path);

	if (parent == NULL) // no parents
		return 0;

	if (is_tc(parent))
		rc = 1;

	free(parent);

	return rc;	
}
