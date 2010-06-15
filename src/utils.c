#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdarg.h>
#include <sys/time.h>
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
	char *l = NULL;

	if (path == NULL)
		return NULL;
	
	if ((l = strrchr(path, '/')) != NULL)
		return l+1;

	return NULL;
}

int file_exists(const char *path)
{
	//struct stat stbuf;
/*	
	if (access (path, F_OK) != -1)
		return 1;
*/

	int f = open(path, O_RDONLY);

	if (f < 0 )
		return 0;

	close(f);	
	/* 
	if (stat(path, &stbuf) != -1)
		if (S_ISREG(stbuf.st_mode))
			return 1;
 	*/
	return 1;
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

#define DEBUG 1

#if DEBUG
void debug(const char *format, ...) 
{
	va_list va;

	va_start(va, format);

	vfprintf(stderr, format, va);
	fprintf(stderr, "\n");

	va_end(va);
}
#else
void debug(const char *format, ...) {}
#endif
