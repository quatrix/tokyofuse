#include <check.h>
#include <sys/mman.h>
#include <tchdb.h>	
#include <fcntl.h>
#include <string.h>
#include "tc_backend.h"

static int wake_up_counter = 0;
static TCHDB *hdb = NULL;

static const char const *tc_test_file = "tc_backend_test.tc";
static const char const *fake_tc_file = "some_fake_tc_file.tc";

static const char *excpected_key_value[4][2] = { { "liron", "makaron" },
											 { "vova" , "vasya"   },
											 { "pita" , "good"    },
											 { "foo"  , "bar"     }};  
					

// mocked 
int tc_gc_wake_up(void)
{
	wake_up_counter++;

	return 1;
}

void setup(void)
{
	int f;

	f = open(fake_tc_file, O_WRONLY | O_CREAT, 0644);

	if (f < 0) {
		fprintf(stderr, "can't create %s needed for testing\n", fake_tc_file);
		exit(1);
	}

	close(f);

}

void teardown(void)
{
	unlink(fake_tc_file);
}


START_TEST(test_tc_open)
{
	fail_unless(tc_open(NULL) == NULL, "opening null file should return null");
	fail_unless(tc_open("/") == NULL, "opening a directory should return null");
	fail_unless(tc_open("/dev/null") == NULL, "opening a device should return null");
	fail_unless(tc_open(fake_tc_file) == NULL, "opening a regular file should return null");

	hdb = tc_open(tc_test_file);
	fail_if(hdb == NULL, "opening valid file shouldn't return null");
}
END_TEST

START_TEST(test_tc_get_next)
{
	const char *fetched_data;
	char *current_key = NULL;
	char *last_key = NULL;
	int last_key_len = 0;

	int fetched_data_len = 0;
	int current_key_len = 0;

	fail_if(hdb == NULL, "hdb should not be null");
	int i = 0;
	int failures = 0;

	for (;;) {
		current_key = tc_get_next(hdb, tc_test_file, 
						last_key, last_key_len, 
						&current_key_len, &fetched_data,  
						&fetched_data_len);

		if (last_key != NULL)
			free(last_key);
	
		if (current_key != NULL) {
			last_key = current_key;
			last_key_len = current_key_len;

			if (strncmp(current_key, excpected_key_value[i][0], strlen(excpected_key_value[i][0])) != 0)
				failures++;
			
			if (strncmp(fetched_data, excpected_key_value[i][1], strlen(excpected_key_value[i][1])) != 0)
				failures++;

			i++;
		}
		else 
			break;
		
	}

	fail_unless(failures == 0, "all read keys and values should match excpected keys values");
	fail_unless(tc_get_next(NULL, NULL, NULL, 0, &current_key_len, &fetched_data, &fetched_data_len) == NULL, "getting next will all nulls should return null");
	fail_unless(tc_get_next(hdb, NULL, "no such key", 12, &current_key_len, &fetched_data, &fetched_data_len) == NULL, "getting next for non existent key should return null");


}
END_TEST

START_TEST(test_tc_get_filesize)
{
	int i, size;
	int failures = 0 ;
	for (i = 0; i < 4; i++) {
		size = tc_get_filesize(hdb, tc_test_file, excpected_key_value[i][0], strlen(excpected_key_value[i][0]) );
		if (size != strlen(excpected_key_value[i][1]))
			failures++;
	}

	fail_unless(failures == 0, "all sizes should be as expected");
	fail_unless(tc_get_filesize(hdb, tc_test_file, "no such key", strlen("no such key")) == -1, "non existent keys should return -1");
	fail_unless(tc_get_filesize(hdb, tc_test_file, NULL, 0) == -1, "NULL keys should return -1");
	fail_unless(tc_get_filesize(NULL, tc_test_file, "liron", strlen("liron")) == -1, "NULL hdb should return -1");
}
END_TEST

START_TEST(test_tc_get_value)
{
	int i, value_len;
	char *value;
	int failures = 0 ;
	for (i = 0; i < 4; i++) {
		value = tc_get_value(hdb, tc_test_file, excpected_key_value[i][0], strlen( excpected_key_value[i][0]), &value_len);
		if (strncmp(value, excpected_key_value[i][1], strlen(excpected_key_value[i][1]) == 0))
			failures++;

		if (value_len != strlen(excpected_key_value[i][1]))
			failures++;
	}

	fail_unless(failures == 0, "all sizes should be as expected");
	fail_unless(tc_get_value(hdb, tc_test_file, "no such key", strlen("no such key"), &value_len) == NULL, "non existent keys should return null");
	fail_unless(tc_get_value(hdb, tc_test_file, NULL, 0, &value_len) == NULL, "null keys should return null");
	fail_unless(tc_get_value(hdb, tc_test_file, "liron", strlen("liron"), NULL) == NULL, "null value_len should return null");
	fail_unless(tc_get_value(NULL, tc_test_file, "liron", strlen("liron"), &value_len) == NULL, "null hdb should return null");
}
END_TEST

START_TEST(test_tc_close)
{

	fail_unless(tc_close(hdb, tc_test_file), "closing an open hdb should return 1");
	fail_if(tc_close(NULL, tc_test_file), "closing NULL hdb should return 0");
}
END_TEST


START_TEST(test_wake_up_counter)
{
	char *mapped = NULL;
	int f;

	fail_if(wake_up_counter > 0, "no one should have called the wake_up_gc function");

	f = open(tc_test_file, O_RDONLY);

	fail_if(f <= 0, "couldn't open file needed to completed test");


	do {
		mapped = mmap(NULL, 1, PROT_READ, MAP_PRIVATE, f, 0);
	} while (mapped != MAP_FAILED);

	close(f);
	
	hdb = tc_open(tc_test_file);
	fail_unless(hdb == NULL, "while we can't allocate memory, this should return null");

	fail_unless(wake_up_counter == TC_CABINET_TRIES, "number of retries should be as defined by TC_CABINET_TRIES");
	
}
END_TEST



Suite *local_suite(void)
{
	Suite *s = suite_create("tc_backend");

	TCase *tc = tcase_create("tc_backend");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_tc_open);
	tcase_add_test(tc, test_tc_get_next);
	tcase_add_test(tc, test_tc_get_filesize);
	tcase_add_test(tc, test_tc_get_value);
	tcase_add_test(tc, test_tc_close);
	tcase_add_test(tc, test_wake_up_counter);
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

