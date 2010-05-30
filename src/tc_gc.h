#ifndef TC_GC_H
#define TC_GC_H

void tc_gc(void *);
int tc_gc_destroy(void);
int tc_gc_init(int);
int tc_gc_wake_up(void);

#endif
