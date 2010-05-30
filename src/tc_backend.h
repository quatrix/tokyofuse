#ifndef TC_BACKEND_H
#define TC_BACKEND_H
#include <tchdb.h>
	

#define TC_CABINET_TRIES 500 // retries for tchdb* functions
#define TC_CABINET_USLEEP 30 // micro seconds

TCHDB *tc_open(const char *);
char *tc_get_next(TCHDB *, const char *, char *,int , int *, const char **, int *);
int tc_get_filesize(TCHDB *, const char *, const char *);
char *tc_get_value(TCHDB *, const char *,  const char *, int *);
int tc_close(TCHDB *, const char *);


#endif
