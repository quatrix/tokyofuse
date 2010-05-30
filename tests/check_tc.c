#include "main.c"
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include "tc.h"


static const char const *fake_tc_file = "some_fake_tc_file.tc";

START_TEST(test_to_tc_path)
{
	char tc_path[100];

	fail_unless(strcmp(to_tc_path("/hey/ho", tc_path), "/hey/ho.tc") == 0, "/hey/ho tc path is /hey/ho.tc");
	fail_unless(strcmp(to_tc_path("", tc_path), ".tc") == 0, "empty tc path is .tc");
	fail_unless(to_tc_path(NULL, tc_path) == NULL, "NULL tc path is NULL");
}
END_TEST

START_TEST(test_tc_dir_stat)
{
	struct stat stbuf;

	tc_dir_stat(&stbuf);

	fail_unless(stbuf.st_nlink == 2, "st_nlink should be 2");
	fail_unless(stbuf.st_mode == (S_IFDIR | 0555), "st_mode should be S_IFDIR | 0555");
	
}
END_TEST

START_TEST(test_tc_file_stat)
{
	struct stat stbuf;
	int size = 5431;

	tc_file_stat(&stbuf, size);
	fail_unless(stbuf.st_nlink == 1, "st_nlink should be 1");
	fail_unless(stbuf.st_mode == (S_IFREG | 0444), "st_mode should be S_IFREG | 0444");
	fail_unless(stbuf.st_size == size, "st_size should be %d", size );
}
END_TEST

START_TEST(test_is_tc)
{
	fail_unless(is_tc("some_fake_tc_file"), "some_fake_tc_file is tc");
	fail_if(is_tc("some_fake_tc"), "some_fake_tc isn't tc");
	fail_if(is_tc("fake_tc_file"), "fake_tc_file isn't tc");
	fail_if(is_tc(""), "\"\" isn't tc");
	fail_unless(is_tc(NULL) == 0, "NULL path should return 0");

}
END_TEST

START_TEST(test_is_parent_tc)
{
	fail_unless(is_parent_tc("some_fake_tc_file/baba"), "some_fake_tc_file/baba has tc parent");
	fail_if(is_parent_tc("some_fake_tc_file/baba/pita"), "some_fake_tc_file/baba/pita doesn't have a tc parent");
	fail_if(is_parent_tc("some_fake_tc_file"), "some_fake_tc_file doesn't have a tc parent");
	fail_unless(is_parent_tc(NULL) == 0, "NULL path should return 0");


}
END_TEST

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

Suite *local_suite(void)
{
	Suite *s = suite_create("tc");

	TCase *tc = tcase_create("tc");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_to_tc_path);
	tcase_add_test(tc, test_tc_dir_stat);
	tcase_add_test(tc, test_tc_file_stat);
	tcase_add_test(tc, test_is_tc);
	tcase_add_test(tc, test_is_parent_tc);
	suite_add_tcase(s, tc);

	return s;
}

