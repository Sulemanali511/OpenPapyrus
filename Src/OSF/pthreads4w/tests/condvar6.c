/*
 * File: condvar6.c
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads for Windows
 *      Copyright 1998 John E. Bossom
 *      Copyright 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * --------------------------------------------------------------------------
 *
 * Test Synopsis:
 * - Test pthread_cond_broadcast.
 *
 * Test Method (Validation or Falsification):
 * - Validation
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Description:
 * - Test broadcast with NUMTHREADS (=5) waiting CVs.
 *
 * Environment:
 * -
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - No output on success.
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */
#include "test.h"
/*
 * Create NUMTHREADS threads in addition to the Main thread.
 */
enum {
	NUMTHREADS = 5
};

static bag_t threadbag[NUMTHREADS + 1];

static cvthing_t cvthing = {
	PTHREAD_COND_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	0
};

static pthread_mutex_t start_flag = PTHREAD_MUTEX_INITIALIZER;
static struct timespec abstime, reltime = { 5, 0 };
static int awoken;

static void * mythread(void * arg)
{
	bag_t * bag = static_cast<bag_t *>(arg);
	assert(bag == &threadbag[bag->threadnum]);
	assert(bag->started == 0);
	bag->started = 1;
	/* Wait for the start gun */
	assert(pthread_mutex_lock(&start_flag) == 0);
	assert(pthread_mutex_unlock(&start_flag) == 0);
	assert(pthread_mutex_lock(&cvthing.lock) == 0);
	while(!(cvthing.shared > 0))
		assert(pthread_cond_timedwait(&cvthing.notbusy, &cvthing.lock, &abstime) == 0);
	assert(cvthing.shared > 0);
	awoken++;
	assert(pthread_mutex_unlock(&cvthing.lock) == 0);
	return (void*)0;
}

int main()
{
	int failed = 0;
	int i;
	pthread_t t[NUMTHREADS + 1];
	cvthing.shared = 0;
	assert((t[0] = pthread_self()).p != NULL);
	assert(cvthing.notbusy == PTHREAD_COND_INITIALIZER);
	assert(cvthing.lock == PTHREAD_MUTEX_INITIALIZER);
	assert(pthread_mutex_lock(&start_flag) == 0);
	(void)pthread_win32_getabstime_np(&abstime, &reltime);
	assert((t[0] = pthread_self()).p != NULL);
	awoken = 0;
	for(i = 1; i <= NUMTHREADS; i++) {
		threadbag[i].started = 0;
		threadbag[i].threadnum = i;
		assert(pthread_create(&t[i], NULL, mythread, (void*)&threadbag[i]) == 0);
	}
	/*
	 * Code to control or manipulate child threads should probably go here.
	 */
	assert(pthread_mutex_unlock(&start_flag) == 0);
	Sleep(1000); // Give threads time to start.
	assert(pthread_mutex_lock(&cvthing.lock) == 0);
	cvthing.shared++;
	assert(pthread_mutex_unlock(&cvthing.lock) == 0);
	assert(pthread_cond_broadcast(&cvthing.notbusy) == 0);
	// Give threads time to complete.
	for(i = 1; i <= NUMTHREADS; i++) {
		assert(pthread_join(t[i], NULL) == 0);
	}
	/*
	 * Cleanup the CV.
	 */
	assert(pthread_mutex_destroy(&cvthing.lock) == 0);
	assert(cvthing.lock == NULL);
	assert(pthread_cond_destroy(&cvthing.notbusy) == 0);
	assert(cvthing.notbusy == NULL);
	// Standard check that all threads started.
	for(i = 1; i <= NUMTHREADS; i++) {
		failed = !threadbag[i].started;
		if(failed) {
			fprintf(stderr, "Thread %d: started %d\n", i, threadbag[i].started);
		}
	}
	assert(!failed);
	// Check any results here.
	assert(awoken == NUMTHREADS);
	return 0; // Success
}
