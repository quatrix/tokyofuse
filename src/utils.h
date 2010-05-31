#ifndef UTILS_H
#define UTILS_H

char *parent_path(const char *);
char *leaf_file(const char *);
int file_exists(const char *);
int has_suffix(const char *, const char *);
char *remove_suffix(char *, const char *);
char *get_caller(void);
struct timespec *future_time(struct timespec *, size_t );
void debug(const char *, ...);

#endif
