#include <check.h>
#include <stdlib.h>
#include <pthread.h>
#include "metadata.h"
#include "tc_dir.h"


static int wake_up_counter = 0;

static const char const *tc_test_file = "tc_backend_test";
static const char const *tc_test_file2 = "tc_backend_test2";
static const char const *some_key = "some_key";

static const char const *fake_tc_file = "some_fake_tc_file";

static const char *excpected_key_value[4][2] = { { "liron", "makaron" },
											 { "vova" , "vasya"   },
											 { "pita" , "good"    },
											 { "foo"  , "bar"     }};  
					
static const char *excpected_key_value2[2][2] = { { "hey", "ho" }, { "lets"  , "go" }};  

static tc_dir_meta_t *tc_dir = NULL;
static tc_dir_meta_t *tc_dir2 = NULL;
static tc_dir_meta_t *t = NULL;

// mocked 
int tc_gc_wake_up(void)
{
	wake_up_counter++;

	return 1;
}

START_TEST(test_metadata_init)
{
	fail_unless(metadata_init(), "initializing metadata");
}
END_TEST


START_TEST(test_metadata_get_tc)
{
	fail_unless(metadata_get_tc(NULL) == NULL, "metadata_get_tc(NULL) should return null");
	fail_unless(metadata_get_tc(fake_tc_file) == NULL, "get_tc for non tc path should return null");

	tc_dir = metadata_get_tc(tc_test_file);

	fail_if(tc_dir == NULL, "valid tc path should return tc_dir");
	fail_unless(strcmp(tc_dir->path, tc_test_file) == 0, "should have correct path");
	fail_unless(tc_dir->initialized == 1,  "new tc_dir should be initialized");
	fail_unless(tc_dir->refcount == 1,  "new tc_dir should have refcount of one");
	fail_unless(tc_dir_lock(tc_dir), "should be able to lock unlocked tc_dir" );
	fail_unless(tc_dir_unlock(tc_dir), "should be able to unlock locked tc_dir" );

	fail_unless(metadata_get_tc(tc_test_file) == tc_dir, "getting the same path again should return same tc_dir");
	fail_unless(tc_dir->refcount == 2,  "getting it again should increment refcount");
	fail_unless(tc_dir_lock(tc_dir), "should be able to lock unlocked tc_dir" );
	fail_unless(tc_dir_unlock(tc_dir), "should be able to unlock locked tc_dir" );

	tc_dir2 = metadata_get_tc(tc_test_file2);

	fail_if(tc_dir2 == NULL, "valid tc path should return tc_dir (2)");
	fail_if(tc_dir == tc_dir2, "getting another path should return a different objects");
	fail_unless(strcmp(tc_dir2->path, tc_test_file2) == 0, "should have correct path (2)");
	fail_unless(tc_dir2->initialized == 1,  "new tc_dir should be initialized");
	fail_unless(tc_dir2->refcount == 1,  "new tc_dir should have refcount of one");
	fail_unless(tc_dir_lock(tc_dir2), "should be able to lock unlocked tc_dir" );
	fail_unless(tc_dir_unlock(tc_dir2), "should be able to unlock locked tc_dir" );

}
END_TEST

START_TEST(test_metadata_add_to_hash)
{
	//t = tc_dir_allocate(some_key);
	t = (tc_dir_meta_t *)malloc(sizeof(tc_dir_meta_t));
	fail_unless(pthread_mutex_init(&t->lock, NULL) == 0, "adding mutex to t");
	fail_unless(pthread_mutex_destroy(&t->lock) == 0, "adding mutex to t");

	fail_if(t == NULL, "failed to malloc new tc_dir_meta_t needed for test");

	t->path = strdup(some_key);
	t->hdb = NULL;
	t->initialized = 0;
	t->refcount = 0;

	//fail_unless(tc_dir_unlock(t), "failed to unlock tc_dir t needed for test");
	
	fail_unless(metadata_add_to_hash(t), "inserting a key into hash should return true");

}
END_TEST

START_TEST(test_metadata_lookup_path)
{
	tc_dir_meta_t *tc_dir_lookup = NULL;
	tc_dir_meta_t *tc_dir2_lookup = NULL;
	tc_dir_meta_t *t_lookup = NULL;

	fail_unless(metadata_lookup_path(tc_test_file, &tc_dir_lookup, TC_LOCK_READ) == TC_EXISTS, "looking up existent tc_dir should return TC_EXISTS");
	fail_unless(metadata_lookup_path(tc_test_file2, &tc_dir2_lookup, TC_LOCK_READ) == TC_EXISTS, "looking up another existent tc_dir should return TC_EXISTS");
	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_READ) == TC_ERROR, "looking up key that can't be locked TC_ERROR");

	fail_unless(pthread_mutex_init(&t->lock, NULL) == 0, "adding mutex to t");

	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_READ) == TC_NOT_FOUND, "looking up key that isn't initialized should return TC_NOT_FOUND");

	t->initialized = 1;

	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_WRITE | TC_LOCK_TRY) == TC_EXISTS, "after initializing it, should return TC_EXISTS");

	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_WRITE | TC_LOCK_TRY | TC_LOCK_DONT_UNLOCK) == TC_EXISTS, "(2) after initializing it, should return TC_EXISTS");
	fail_if(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "lookup shouldn't have released write lock");
	fail_unless(metadata_unlock(), "should be able to unlock");

	fail_unless(t->refcount == 2, "after two lookups on refcount=0 object, refcount should be 2");

	fail_unless(tc_dir_lookup == tc_dir, "looking path should return correct tc_dir");
	fail_unless(tc_dir2_lookup == tc_dir2, "looking other path should return correct tc_dir(2)");

	fail_unless(strcmp(tc_dir->path, tc_test_file) == 0, "should have correct path");
	fail_unless(tc_dir->refcount == 3,  "lookup should increment refcount");
	fail_unless(tc_dir_lock(tc_dir), "should be able to lock unlocked tc_dir" );
	fail_unless(tc_dir_unlock(tc_dir), "should be able to unlock locked tc_dir" );

	fail_unless(strcmp(tc_dir2->path, tc_test_file2) == 0, "should have correct path (2)");
	fail_unless(tc_dir2->refcount == 2,  "lookup should increment other file too");
	fail_unless(tc_dir_lock(tc_dir2), "should be able to lock unlocked tc_dir" );
	fail_unless(tc_dir_unlock(tc_dir2), "should be able to unlock locked tc_dir" );


}
END_TEST


START_TEST(test_metadata_get_filesize)
{
	int i, size;
	char key[50];

	for (i = 0; i < 4; i++) {
		sprintf(key, "%s/%s", tc_test_file, excpected_key_value[i][0]);

		size = metadata_get_filesize(key);

		fail_unless(size == strlen(excpected_key_value[i][1]), "get size: wrong size %d (expected: %d)", size, excpected_key_value[i][1]);
	}

	for (i = 0; i < 2; i++) {
		sprintf(key, "%s/%s", tc_test_file2, excpected_key_value2[i][0]);

		size = metadata_get_filesize(key);

		fail_unless(size == strlen(excpected_key_value2[i][1]), "(2) get size: wrong size %d (expected: %d)", size, excpected_key_value2[i][1]);
	}

	fail_unless(tc_dir->refcount == 3, "after fetching size refcount should not change");
	fail_unless(tc_dir2->refcount == 2, "(2) after fetching size refcount should not change");

	fail_unless(metadata_get_filesize("some path") == -1, "for non existent path return -1");
	fail_unless(metadata_get_filesize(NULL) == -1, "for NULL path return -1");

}
END_TEST



START_TEST(test_metadata_get_value)
{
	tc_filehandle_t fh;
	int i, size;
	char key[50];

	for (i = 0; i < 4; i++) {
		sprintf(key, "%s/%s", tc_test_file, excpected_key_value[i][0]);

		fail_unless(metadata_get_value(key, &fh));

		fail_unless(fh.value_len == strlen(excpected_key_value[i][1]), 
			"get value: wrong value_len %d (excpected: %d)", fh.value_len, excpected_key_value[i][1]);

		fail_unless(strcmp(fh.value, excpected_key_value[i][1]) == 0, 
			"get value: wrong value %s (excpected: %s)", fh.value, excpected_key_value[i][1]);
	}

	for (i = 0; i < 2; i++) {
		sprintf(key, "%s/%s", tc_test_file2, excpected_key_value2[i][0]);

		fail_unless(metadata_get_value(key, &fh));

		fail_unless(fh.value_len == strlen(excpected_key_value2[i][1]), 
			"(2) get value: wrong value_len %d (excpected: %d)", fh.value_len, excpected_key_value2[i][1]);

		fail_unless(strcmp(fh.value, excpected_key_value2[i][1]) == 0, 
			"(2) get value: wrong value %s (excpected: %s)", fh.value, excpected_key_value2[i][1]);
	}

	fail_unless(tc_dir->refcount == 7, "after fetching 4 values refcount should be 7 but it's %d", tc_dir->refcount);
	fail_unless(tc_dir2->refcount == 4, "after fetching 2 values refcount should be 4 but it's %d", tc_dir2->refcount);

	fail_unless(metadata_get_value("some path", &fh) == 0, "for non existent path return 0");
	fail_unless(metadata_get_value(NULL, &fh) == 0, "for NULL path return 0");
	fail_unless(metadata_get_value("tc_backend_test/foo", NULL) == 0, "for NULL fh return 0");

}
END_TEST



START_TEST(test_metadata_release_path)
{
	int i;

	fail_if(metadata_release_path(NULL), "release path should return false");

	for (i = 7; i != 0; i-- ) {
		fail_unless(metadata_release_path(tc_dir), "release path shouldn't return NULL");
		fail_unless(tc_dir->refcount == (i-1), "refcount should be %d while it's %d", i, tc_dir->refcount);
	}

}
END_TEST


static void *try_lock_f(void *data)
{
	TC_LOCKTYPE lock = (TC_LOCKTYPE)data;
	int rc;

	rc = metadata_lock(lock);

	if (rc) {
		usleep(500);
		rc = metadata_unlock();
	}

	pthread_exit((void *)rc);
}

START_TEST(test_metadata_lock)
{
	pthread_t try_lock;
	pthread_attr_t attr;
	void *rc; 

	fail_unless(metadata_lock(TC_LOCK_READ), "should be able to read lock");
	fail_unless(metadata_lock(TC_LOCK_READ), "should be able to read lock again");
	fail_if(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "shouldn't be able to write lock a read locked lock");
	fail_unless(metadata_unlock(), "should be able to unlock");
	fail_unless(metadata_unlock(), "should be able to unlock again");
	fail_unless(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "should be able write lock unlocked thread");
	fail_if(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "shouldn't be able write lock already locked thread");


	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	fail_unless(pthread_create(&try_lock, &attr, (void *)try_lock_f, (void *)(TC_LOCK_WRITE | TC_LOCK_TRY) ) == 0, "need to start another thread to check lock");


	pthread_join(try_lock, &rc);

	fail_if((int)rc, "thread shouldn't be able to gain lock");

	fail_unless(metadata_unlock(), "should be able to unlock again");

	fail_unless(pthread_create(&try_lock, &attr, (void *)try_lock_f, (void *)(TC_LOCK_WRITE | TC_LOCK_TRY) ) == 0, "need to start another thread to check lock");
	
	usleep(100);
	fail_if(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "lock should be hald by other thread");


	pthread_join(try_lock, &rc);

	fail_unless((int)rc, "thread should be able to gain lock");

	fail_unless(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "lock thread should have released the lock");

	fail_unless(metadata_unlock(), "should be able to unlock again");


	fail_unless(pthread_create(&try_lock, &attr, (void *)try_lock_f, (void *)(TC_LOCK_READ | TC_LOCK_TRY) ) == 0, "need to start another thread to check lock");
	usleep(100);

	fail_unless(metadata_lock(TC_LOCK_READ | TC_LOCK_TRY), "should be able to read lock");


   	pthread_attr_destroy(&attr);
	pthread_join(try_lock, &rc);

	fail_unless((int)rc, "thread should be able to gain lock");

	fail_unless(metadata_unlock(), "should be able to unlock again");
	fail_unless(metadata_lock(TC_LOCK_WRITE | TC_LOCK_TRY), "lock thread should have released the lock");
	fail_unless(metadata_unlock(), "should be able to unlock again");


}
END_TEST

START_TEST(test_metadata_free_unused_tc_dir)
{
	metadata_free_unused_tc_dir();
	tc_dir_meta_t *tc_dir_lookup = NULL;
	tc_dir_meta_t *t_lookup = NULL;
	tc_filehandle_t fh;
	int i, size;
	char key[50];

	fail_unless(metadata_lookup_path(tc_test_file, &tc_dir_lookup, TC_LOCK_READ | TC_LOCK_TRY) == TC_NOT_FOUND, "looking up object that should be freed");
	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_READ | TC_LOCK_TRY) == TC_EXISTS, "but objects with higher refcount should stay");
	fail_unless(t_lookup->refcount == 3, "refcount should be 3");

	t_lookup->refcount = 1;
	metadata_free_unused_tc_dir();

	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_READ | TC_LOCK_TRY) == TC_EXISTS, "but objects with higher refcount should stay");
	fail_unless(t_lookup->refcount == 2, "refcount should be 2");

	t_lookup->refcount = 0;
	metadata_free_unused_tc_dir();

	fail_unless(metadata_lookup_path(some_key, &t_lookup, TC_LOCK_READ | TC_LOCK_TRY) == TC_NOT_FOUND, "refcount 0 object should have been freed");

	for (i = 0; i < 2; i++) {
		sprintf(key, "%s/%s", tc_test_file2, excpected_key_value2[i][0]);

		fail_unless(metadata_get_value(key, &fh));

		fail_unless(fh.value_len == strlen(excpected_key_value2[i][1]), 
			"get value: wrong value_len %d (excpected: %d)", fh.value_len, excpected_key_value2[i][1]);

		fail_unless(strcmp(fh.value, excpected_key_value2[i][1]) == 0, 
			"get value: wrong value %s (excpected: %s)", fh.value, excpected_key_value2[i][1]);
	}

	fail_unless(tc_dir2->refcount == 6, "after fetching 2 values refcount should be 6 but it's %d", tc_dir2->refcount);

}
END_TEST



Suite *local_suite(void)
{
	Suite *s = suite_create("metadata");

	TCase *tc = tcase_create("metadata");
	tcase_add_test(tc, test_metadata_init);
	tcase_add_test(tc, test_metadata_get_tc);
	tcase_add_test(tc, test_metadata_add_to_hash);
	tcase_add_test(tc, test_metadata_lookup_path);
	tcase_add_test(tc, test_metadata_get_filesize);
	tcase_add_test(tc, test_metadata_get_value);
	tcase_add_test(tc, test_metadata_release_path);
	tcase_add_test(tc, test_metadata_lock);
	tcase_add_test(tc, test_metadata_free_unused_tc_dir);
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

