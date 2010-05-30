#include <check.h>
#include <stdlib.h>

START_TEST(test_metadata_init)
{
}
END_TEST

Suite *local_suite(void)
{
	Suite *s = suite_create("metadata");

	TCase *tc = tcase_create("metadata");
	tcase_add_test(tc, test_metadata_init);
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

