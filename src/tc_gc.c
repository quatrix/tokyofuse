#include <pthread.h>
#include "metadata.h"
#include "tc_gc.h"
#include "utils.h"



static int tc_gc_sleep;
static pthread_mutex_t gc_mutex;
static pthread_cond_t gc_cond;
static pthread_t gc_thread;


int tc_gc_init(int gc_sleep)
{
	if ((tc_gc_sleep = gc_sleep) <= 0) {
		debug("init_tc_gc: gc_sleep must be above 0 seconds");
		return 0;
	}


	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if (gc_sleep < 0) {
		debug("init_tc_gc: gc_sleep must be above zero");
		return 0;
	}

	if (pthread_mutex_init(&gc_mutex, NULL) != 0) {
		debug("init_tc_gc: can't init mutex gc_mutex");
		return 0;
	}

  	if (pthread_cond_init(&gc_cond, NULL) != 0) {
		debug("init_tc_gc: can't init condition gc_cond");
		return 0;
	}

	if (pthread_create(&gc_thread,  NULL, (void *)tc_gc, NULL) != 0) {
		debug("init_tc_gc: failed to create gc thread");
		return 0;
	}



	return 1;	
}

int tc_gc_destroy(void)
{
	debug("tc_gc_destroy: killing gc");

	if (pthread_cancel(gc_thread) != 0) {
		debug("tc_gc_destroy: failed to cancel gc_thread");
		return 0;
	}

	debug("tc_gc_destroy: joining gc thread");
	if (pthread_join(gc_thread, NULL) != 0) {
		debug("tc_gc_destroy: failed to join gc thread");
		return 0;
	}
	else
		debug("tc_gc_destroy: gc thread joined");

	return 1;
}

int tc_gc_wake_up(void)
{
	debug("STARVING MARVIN!!!");

	if (pthread_mutex_lock(&gc_mutex) != 0)
		return 0;

	if (pthread_cond_signal(&gc_cond) != 0)
		return 0;

	pthread_mutex_unlock(&gc_mutex);

	return 1;
}

void tc_gc(void *data)
{
		struct timespec ts;

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

		while (true) {
			if (pthread_mutex_lock(&gc_mutex) != 0)
				continue;

			pthread_cond_timedwait(&gc_cond, &gc_mutex, future_time(&ts, tc_gc_sleep));

			debug("tc_gc: ENTER THE COLLECTOR!!"); // always wanted to say that
	
			metadata_free_unused_tc_dir();
			
			pthread_mutex_unlock(&gc_mutex);
			pthread_testcancel();
		}

		debug("gc loop exited");
}
