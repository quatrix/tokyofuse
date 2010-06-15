#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "tc_dir.h"

#define LOCK_SLEEP 5000
#define GRACE_SLEEP 1000

static const char *tc_test_file     = "tc_backend_test";
static const char *tc_test_file_tc  = "tc_backend_test.tc";
static const char *fake_tc_file     = "fake_tc_file";
static tc_dir_meta_t *tc_dir        = NULL;
static tc_dir_meta_t *tc_dir_broken = NULL;


static int tc_dir_refcount = 0; 

int tc_open(const char *path)
{
	if (strcmp(path, tc_test_file_tc) == 0)
		return 1;

	return 0;
}

int tc_close(void *hdb, const char *path)
{


	return 0;
}


START_TEST(test_tc_dir_allocate)
{
	fail_unless(tc_dir_allocate(NULL) == NULL, "allocating with null path should return null");

	tc_dir = tc_dir_allocate(tc_test_file);
	tc_dir_broken = tc_dir_allocate(fake_tc_file);

	fail_if(tc_dir == NULL, "tc_dir should be allocated");
	fail_if(tc_dir_broken == NULL, "tc_dir_broken should be allocated");

	fail_unless(strcmp(tc_dir->path, tc_test_file) == 0, "%s should be %s", tc_dir->path, tc_test_file);
	fail_unless(strcmp(tc_dir_broken->path, fake_tc_file) == 0, "%s should be %s", tc_dir_broken->path, fake_tc_file);

	fail_unless(tc_dir->initialized == 0, "shouldn't be initialized");
	fail_unless(tc_dir->hdb == NULL, "tc_dir->hdb should be null");
	fail_unless(tc_dir->refcount == 0, "refcount zero");

	fail_unless(tc_dir_broken->initialized == 0, "shouldn't be initialized");
	fail_unless(tc_dir_broken->hdb == NULL, "tc_dir_broken->hdb should be null");
	fail_unless(tc_dir_broken->refcount == 0, "refcount zero");

	fail_if(pthread_mutex_trylock(&tc_dir->lock) == 0, "shouldn't be able to lock a locked dir");
	fail_if(pthread_mutex_trylock(&tc_dir_broken->lock) == 0, "shouldn't be able to lock a locked dir");

}
END_TEST

START_TEST(test_tc_dir_init)
{
	fail_unless(tc_dir_init(tc_dir) == tc_dir, "should be able to init this tc_dir");
	fail_unless(tc_dir->initialized, "should be initialized");
	
	tc_dir_refcount++;

	fail_unless(tc_dir->refcount == tc_dir_refcount,  "tc_dir->refcount (%d) should be %d", tc_dir->refcount, tc_dir_refcount);
	fail_unless(tc_dir_unlock(tc_dir), "tc_dir unlocking should work");
	fail_unless(pthread_mutex_trylock(&tc_dir->lock) == 0, "should be able to lock an unlocked dir");
	fail_unless(tc_dir_unlock(tc_dir), "tc_dir unlocking should work");

	fail_unless(tc_dir_init(tc_dir_broken) == NULL, "bad path should return null tc_dir");
	fail_unless(tc_dir_broken->initialized == 0, "shouldn't be initialized");
	fail_unless(tc_dir_broken->hdb == NULL, "tc_dir_broken->hdb should be null");
	fail_unless(tc_dir_broken->refcount == 0, "refcount zero");
	fail_unless(tc_dir_unlock(tc_dir_broken), "tc_dir_broken unlocking should work");
	fail_unless(pthread_mutex_trylock(&tc_dir_broken->lock) == 0, "should be able to lock an unlocked dir");
	fail_unless(tc_dir_unlock(tc_dir_broken), "tc_dir_broken unlocking should work");

}
END_TEST



START_TEST(test_tc_dir_dec_refcount)
{
	fail_if(tc_dir_dec_refcount(NULL), "can't dec refcount on null tc_dir");
	fail_unless(tc_dir_dec_refcount(tc_dir), "should be able to dec refcount on valid tc_dir");
	tc_dir_refcount--;
	fail_unless(tc_dir->refcount == tc_dir_refcount,  "tc_dir->refcount (%d) should be %d", tc_dir->refcount, tc_dir_refcount);
}
END_TEST

static void *try_lock_f(void *data)
{
	tc_dir_meta_t *tc_dir = (tc_dir_meta_t *)data;
	int rc;

	rc = tc_dir_trylock(tc_dir);

	if (rc) {
		usleep(LOCK_SLEEP);
		rc = tc_dir_unlock(tc_dir);
	}

	pthread_exit((void *)rc);
}

static double time_hires_diff(struct timeval *td, struct timeval *t0)
{
	double start, end;

	start = t0->tv_sec + (double)t0->tv_usec / 1000000;
	end = td->tv_sec + (double)td->tv_usec / 1000000;

	return (end - start) * 1000000;
}
START_TEST(test_tc_dir_lock)
{
	pthread_t try_lock;
	pthread_attr_t attr;
	struct timeval t0, td;
	int elapsed;
	void *rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	fail_unless(tc_dir_lock(tc_dir), "should be able to lock unlocked tc_dir");
	fail_if(tc_dir_trylock(tc_dir), "shouldn't be able to lock locked tc_dir");

	fail_unless(pthread_create(&try_lock, &attr, (void *)try_lock_f, (void *)tc_dir ) == 0, "need to start another thread to check lock");
	
	pthread_join(try_lock, &rc);

	fail_if((int)rc, "thread shouldn't be able to gain lock");

	fail_unless(tc_dir_unlock(tc_dir), "should be able to unlock tc_dir");

	fail_unless(pthread_create(&try_lock, &attr, (void *)try_lock_f, (void *)tc_dir ) == 0, "need to start another thread to check lock");

	gettimeofday(&t0, NULL);

	usleep(GRACE_SLEEP);

	fail_unless(tc_dir_lock(tc_dir), "should wait until lock released and get lock");

	pthread_join(try_lock, &rc);

	gettimeofday(&td, NULL);
	
	elapsed = time_hires_diff(&td, &t0);

	fail_unless((int)rc, "thread should be able to gain lock");

	fail_if(elapsed > (LOCK_SLEEP * 2), "lock was held for (%d) too long (expected not longer than %d)", elapsed, (LOCK_SLEEP * 2));
	fail_if(elapsed < (LOCK_SLEEP / 2), "lock was held for (%d) too little (expected shorter than %d)", elapsed, (LOCK_SLEEP / 2));

	fail_unless(tc_dir_unlock(tc_dir), "should be able to unlock tc_dir");

}
END_TEST


Suite *local_suite(void)
{
	Suite *s = suite_create("tc_dir");

	TCase *tc = tcase_create("tc_dir");
	tcase_add_test(tc, test_tc_dir_allocate);
	tcase_add_test(tc, test_tc_dir_init);
	tcase_add_test(tc, test_tc_dir_dec_refcount);
	tcase_add_test(tc, test_tc_dir_lock);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	SRunner *sr;
	Suite *s;
	int failed;

	s = local_suite();
	sr = srunner_create(s);
	srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all(sr, CK_NORMAL);

	failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

