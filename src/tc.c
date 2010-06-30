#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tc.h"
#include "utils.h"



char *to_tc_path(const char *path, size_t path_len, char *dst_path)
{
	if ((path_len + TC_SUFFIX_LEN) > MAX_PATH_LEN)
		return NULL;

	memcpy(dst_path, path, path_len);
	memcpy(dst_path + path_len, TC_SUFFIX, TC_SUFFIX_LEN);

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

int is_tc(const char *path, size_t path_len) 
{
	if (path == NULL)
		return 0;


	//size_t path_len = strlen(path);
	char tc_path[MAX_PATH_LEN];

	if (to_tc_path(path, path_len, tc_path) == NULL)
		return 0;

	if (file_exists(tc_path))
		return 1;
		
	return 0;
}

int is_parent_tc(const char *path, size_t path_len)
{
	char parent[MAX_PATH_LEN];
	size_t parent_len = 0;
	
	if (path == NULL)
		return 0;

	if (!s_strncpy(parent, path, path_len, MAX_PATH_LEN)) {
		error("safe copy failed, bah");
		return 0;
	}

	parent_len = parent_path(parent, path_len);

	if (!parent_len)
		return 0;

	if (is_tc(parent, parent_len))
		return 1;

	return 0;
}
