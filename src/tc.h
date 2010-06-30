#ifndef TC_H
#define TC_H
#include <sys/stat.h>

#define TC_SUFFIX_LEN 4
#define TC_SUFFIX     ".tc"
#define MAX_PATH_LEN  100

void tc_dir_stat(struct stat *);
void tc_file_stat(struct stat *, size_t);
int is_tc(const char *, size_t);
int is_parent_tc(const char *, size_t);
char *to_tc_path(const char *, size_t, char *);

#endif
