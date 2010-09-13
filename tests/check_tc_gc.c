#include <check.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "tc_gc.h"

#define GC_SLEEP 1
#define RUNS 2

volatile static int counter = 0;

// mocked 
void metadata_free_unused_tc_dir(void)
{
	counter++;
}


START_TEST(test_tc_gc_init)
{
	fail_if(tc_gc_init(0), "shouldn't be allowed to init with 0 seconds");
	fail_unless(tc_gc_init(GC_SLEEP), "returns true on >0 seconds");
}
END_TEST

START_TEST(test_tc_gc_run)
{
	clock_t t0, td, t_start;
	int last_counter = counter;
	double elapsed;

	t_start = t0 = clock();
	

	for (;;) {
		td = clock();
				
		if (counter != last_counter) {
			if (counter == last_counter + 1) {
				
				elapsed = round(((double) (td - t0)) / CLOCKS_PER_SEC);

				fail_unless(elapsed == (double)GC_SLEEP, "time elapsed (%f) should be GC_SLEEP (%d)", elapsed, GC_SLEEP);

				t0 = td;
				last_counter = counter;

				if (counter == RUNS)
					break;
			}
			else 
				fail("counter should have been incremented by one");		

		}

		elapsed = round((double) (td - t_start) / CLOCKS_PER_SEC);
		fail_if(elapsed > RUNS * GC_SLEEP, "waiting for too long (%f)", elapsed);
	}

}
END_TEST

START_TEST(test_tc_gc_wake_up)
{
	int i;
	int last_counter = counter;

	for (i = 0; i < 10; i++) {
		tc_gc_wake_up();

		usleep(100);
		fail_unless(counter == last_counter + 1, "waking up gc should inc counter almost almost instancly");
		last_counter = counter;
	}

}
END_TEST


START_TEST(test_tc_gc_destroy)
{
	fail_unless(tc_gc_destroy());
}
END_TEST

Suite *local_suite(void)
{
	Suite *s = suite_create("tc_gc");

	TCase *tc = tcase_create("tc_gc");
	tcase_add_test(tc, test_tc_gc_init);
	tcase_add_test(tc, test_tc_gc_run);
	tcase_add_test(tc, test_tc_gc_wake_up);
	tcase_add_test(tc, test_tc_gc_destroy);
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

