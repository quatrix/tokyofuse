#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "config.h"


START_TEST(test_external)
{
	int rc;
	rc = system(PYTHON_BIN " external/01_external.py --tc_fuse_binary=../src/tokyofuse");

	fail_unless(rc == 0, "external test failed");

}
END_TEST



Suite *local_suite(void)
{
	Suite *s = suite_create("external");

	TCase *tc = tcase_create("external");
	tcase_add_test(tc, test_external);
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

