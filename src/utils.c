#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "utils.h"


double get_time(void)
{
    struct timeval t;
    struct timezone tzp;
    gettimeofday(&t, &tzp);
    return t.tv_sec + t.tv_usec*1e-6;
}

int s_strncpy(char *dest, const char *src, size_t src_len, size_t len)
{
	/* 
	register size_t i;
	
	for (i = 0; src[i] != '\0' && i < len; i++)
		dest[i] = src[i];

	dest[i] = '\0';

	if (i == len)
		return 0;

	

	return 1;
	*/

	src_len++; // for '\0' char

	if (src_len > len)
		return 0;

	memcpy(dest, src, src_len);

	return 1;
}

size_t parent_path(char *path, size_t path_len)
{
	int i;

	if (path_len == 0)
		return 0;

	i = path_len - 1;

	while (i > 0) {
		i--;
		if (path[i] == '/') {
			path[i] = '\0';
			break;
		}
	}

	return i;
}

char *leaf_file(const char *path, size_t path_len, size_t *leaf_len)
{
	char *l = NULL;

	if (path == NULL)
		return NULL;
	
	if ((l = memrchr(path, '/', path_len)) != NULL) {
		*leaf_len = path_len - (l - path) - 1;
		return l+1;
	}

	return NULL;
}

int file_exists(const char *path)
{
	// /hdsarchive202/tctests/tc/000/000.tc
	//fprintf(stderr, "file_exists: %s\n", path);

/* 
	size_t slash_count = 0;
	const char *path_p = path;
	
	while (*path_p != '\0')
		if (*path_p++ == '/')
			slash_count++;

	if (slash_count == 5)
		return 1;

	return 0;
*/

	if (access (path, F_OK) != -1)
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

char *get_caller(void)
{
	void *array[3];
	size_t size;
	char **strings, *after_path = NULL, *caller = NULL;

	size = backtrace(array, 3);
	strings = backtrace_symbols(array, size);

	if (size > 2) {
		after_path = strchr(strings[2], '(');

		if (after_path != NULL)
			caller = strdup(after_path);
			
	}

	free (strings);

	return caller;
}


struct timespec *future_time(struct timespec *ts, size_t seconds)
{
	struct timeval tp;

	if (gettimeofday(&tp, NULL) != 0)
		return 0;

	ts->tv_sec  = tp.tv_sec;
    ts->tv_nsec = tp.tv_usec * 1000;
    ts->tv_sec += seconds;

	return ts;
}

size_t unique_id(void)
{
	static size_t uid = 0;

	return uid++;
}

