#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "main.c"
#include <sys/time.h>
#include "utils.h"


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
	char *parent = NULL;

	parent = parent_path("/hey/ho/lets");
	fail_unless(strcmp(parent, "/hey/ho") == 0, "parent of /hey/ho/lets is /hey/ho");
	free(parent);
	parent = NULL;

	parent = parent_path("/hey/ho/lets/");
	fail_unless(strcmp(parent, "/hey/ho") == 0, "parent of /hey/ho/lets/ is /hey/ho");
	free(parent);
	parent = NULL;


}
END_TEST


START_TEST(test_leaf_file)
{
	fail_unless(strcmp(leaf_file("/hey/ho"), "ho") == 0, "leaf of /hey/ho is ho");
	fail_unless(strcmp(leaf_file("hey/ho"), "ho") == 0, "leaf of /hey/ho is ho");
	fail_unless(strcmp(leaf_file("/"), "") == 0, "leaf of /hey/ho is ho");
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

	fail_if(gettimeofday(&tp, NULL) != 0);

	future_time(&ts, 0);
	future_time(&ts_after_5, 5);

	fail_unless(tp.tv_sec == ts.tv_sec, "future time 0 should be now");
	fail_unless(ts.tv_sec - ts_after_5.tv_sec != 5, "time should be 5 seconds in the future");
	
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
	suite_add_tcase(s, tc);

	return s;
}

