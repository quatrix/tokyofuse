#ifndef UTILS_H
#define UTILS_H



char *parent_path(const char *);
int file_exists(const char *);
int has_suffix(const char *, const char *);
char *remove_suffix(char *, const char *);

#endif
