#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "main.c"
#include <sys/time.h>
#include "utils.h"


#define MAX_PATH_LEN 100

START_TEST(test_has_prefix)
{
	fail_unless(has_suffix("abcd", "cd"), "abcd ends with cd");
	fail_unless(has_suffix("abcd", "abcd"), "abcd ends with abcd");
	fail_unless(has_suffix("abcd", ""), "everything ends with empty suffixes");
	fail_if(has_suffix("abcd", "ccd"), "abcd doesn't end with ccd");
	fail_if(has_suffix("abcd", "bc"), "abcd doesn't end with bc");
	fail_if(has_suffix("abcd", "aabcd"), "suffix longer than string");
	fail_if(has_suffix("abcd", "cdb"), "abcd doesn't end with cdb");
}
END_TEST


START_TEST(test_parent_path)
{

	char parent[30] = "/hey/ho/lets";
	char *expected_result = "/hey/ho";
	size_t expected_result_len = strlen(expected_result);
	size_t path_len = strlen(parent);
	size_t parent_len;

	parent_len = parent_path(parent, path_len);


	
	fail_unless(strcmp(parent, expected_result) == 0, "parent of /hey/ho/lets is /hey/ho");
	fail_unless(parent_len = expected_result_len );

	strcpy(parent, "/hey/ho/lets/");
	path_len = strlen(parent);

	parent_len = parent_path(parent, path_len);

	fail_unless(strcmp(parent, expected_result) == 0, "parent of /hey/ho/lets is /hey/ho");
	fail_unless(parent_len = expected_result_len );

	strcpy(parent, "/");
	path_len = strlen(parent);
	parent_len = parent_path(parent, path_len);


	fail_unless(parent_len == 0, "/ has no parents (parent_len == %d)", parent_len);

	strcpy(parent, "orphan");
	path_len = strlen(parent);
	parent_len = parent_path(parent, path_len);

	fail_unless(parent_len == 0, "orphan has no parents");

}
END_TEST


START_TEST(test_leaf_file)
{
	fail_unless(strcmp(leaf_file("/hey/ho", strlen("/hey/ho")), "ho") == 0, "leaf of /hey/ho is ho");
	fail_unless(strcmp(leaf_file("hey/ho", strlen("hey/ho")), "ho") == 0, "leaf of /hey/ho is ho");
	fail_unless(strcmp(leaf_file("/", strlen("/")), "") == 0, "leaf of /hey/ho is ho");
	fail_unless(leaf_file("heya", strlen("heya")) == NULL, "leaf of string w/o '/' should return NULL");
	fail_unless(leaf_file(NULL, 0) == NULL, "leaf of NULL should return NULL");
}
END_TEST

START_TEST(test_file_exists)
{

	fail_unless(file_exists("/dev/null"), "/dev/null exists");  
	fail_if(file_exists("/dev/pita"), "/dev/pita doesn't exist");  
	fail_if(file_exists(NULL), "NULL doesn't exist");  
}
END_TEST

START_TEST(test_remove_suffix)
{
	char s1[] = "helloworld";
	fail_unless(strcmp(remove_suffix(s1,"world"), "hello") == 0, "helloworld - world = hello");

	char s2[] = "baba";
	fail_unless(remove_suffix(s2,"world") == NULL, "can't remove world from baba");
}
END_TEST


START_TEST(test_future_time)
{
	struct timeval tp;
	struct timespec ts, ts_after_5;

	fail_if(gettimeofday(&tp, NULL) != 0, "can't get timeofday needed for test");

	future_time(&ts, 0);
	future_time(&ts_after_5, 5);

	fail_unless(tp.tv_sec == ts.tv_sec, "future time 0 should be now");
	fail_unless(ts_after_5.tv_sec - tp.tv_sec == 5, "time should be 5 seconds in the future");
	
}
END_TEST

START_TEST(test_unique_id)
{
	int i;
	for (i = 0; i < 1000; i++)
		fail_unless(unique_id() == i);
}
END_TEST

Suite *local_suite(void)
{
	Suite *s = suite_create("utils");

	TCase *tc = tcase_create("utils");
	tcase_add_test(tc, test_has_prefix);
	tcase_add_test(tc, test_parent_path);
	tcase_add_test(tc, test_leaf_file);
	tcase_add_test(tc, test_file_exists);
	tcase_add_test(tc, test_remove_suffix);
	tcase_add_test(tc, test_future_time);
	tcase_add_test(tc, test_unique_id);
	suite_add_tcase(s, tc);

	return s;
}

