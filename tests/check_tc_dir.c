#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "tc_dir.h"

static const char *tc_test_file = "tc_backend_test";
static const char *fake_tc_file = "fake_tc_file";
static tc_dir_meta_t *tc_dir = NULL;
static tc_dir_meta_t *tc_dir_broken = NULL;


int tc_open(const char *path)
{
	if (strcmp(path, "tc_backend_test.tc") == 0)
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
	fail_unless(tc_dir->refcount == 1, "refcount should be 1");
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

Suite *local_suite(void)
{
	Suite *s = suite_create("tc_dir");

	TCase *tc = tcase_create("tc_dir");
	tcase_add_test(tc, test_tc_dir_allocate);
	tcase_add_test(tc, test_tc_dir_init);
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

