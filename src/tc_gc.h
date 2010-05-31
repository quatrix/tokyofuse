#ifndef TC_GC_H
#define TC_GC_H

void tc_gc(void *);
int tc_gc_destroy(void);
int init_tc_gc(int);
int wake_up_gc(void);

#endif
