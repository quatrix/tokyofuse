#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "utils.h"



char *parent_path(const char *path)
{
	char *tc_path, *p_path;
	size_t tc_path_len;

	tc_path = strdup(path);
	tc_path_len = strlen(tc_path);

	// if directory, get parent directory
	if (tc_path[tc_path_len -1] == '/')
		tc_path[tc_path_len - 1] = '\0';

	p_path = strrchr(tc_path, '/');

	if (p_path != NULL) {
		*(p_path++) = '\0';
		return tc_path;
	}

	return NULL;
}

char *leaf_file(const char *path)
{
	return strrchr(path, '/') + 1;
}

int file_exists(const char *path)
{
	struct stat stbuf;
	
	if (stat(path, &stbuf) != -1)
		if (S_ISREG(stbuf.st_mode))
			return 1;

	return 0;
}

int has_suffix(const char *s, const char *suffix)
{
	size_t s_len = strlen(s);
	size_t suffix_len = strlen(suffix);
	int i;

	if (s_len < suffix_len)
		return 0;

	for (i = 0; suffix_len > 0; i--) {
		if (s[s_len-1] != suffix[suffix_len-1])
			return 0;

		s_len--;
		suffix_len--;
	}
		
	return 1;
}

// FIXME not actually checking if s has suffix
// just removing strlen(suffix) chars of s
char *remove_suffix(char *s, const char *suffix)
{
	size_t s_len = strlen(s);
	size_t suffix_len = strlen(suffix);

	if (s_len < suffix_len)
		return NULL;

	*(s + s_len - suffix_len) = '\0';

	return s;
}
