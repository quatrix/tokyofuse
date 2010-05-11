#ifndef TC_H
#define TC_H
#include <sys/stat.h>

void tc_dir_stat(struct stat *);
void tc_file_stat(struct stat *, size_t);
int is_tc(const char *);
int is_parent_tc(const char *);
char *to_tc_path(const char *);

#endif
