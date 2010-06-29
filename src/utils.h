#ifndef UTILS_H
#define UTILS_H

#define DEBUG 0

#if DEBUG
#define debug(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)
#else
#define debug(format, ...) 
#endif

#define error(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)

char *parent_path(const char *);
char *leaf_file(const char *);
int file_exists(const char *);
int has_suffix(const char *, const char *);
char *remove_suffix(char *, const char *);
char *get_caller(void);
struct timespec *future_time(struct timespec *, size_t );
size_t unique_id(void);

#endif
