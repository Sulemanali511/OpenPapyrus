/*
 * File: cleanup1.c
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
 * Test Synopsis: Test cleanup handler executes (when thread is canceled).
 *
 * Test Method (Validation or Falsification):
 * -
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
 * -
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
 * - have working pthread_create, pthread_self, pthread_mutex_lock/unlock
 *   pthread_testcancel, pthread_cancel, pthread_join
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */
#include "test.h"

#if defined(_MSC_VER) || defined(__cplusplus)
/*
 * Create NUMTHREADS threads in addition to the Main thread.
 */
enum {
	NUMTHREADS = 10
};

static bag_t threadbag[NUMTHREADS + 1];

typedef struct {
	int i;
	CRITICAL_SECTION cs;
} sharedInt_t;

static sharedInt_t pop_count;

static void
#ifdef __PTW32_CLEANUP_C
__cdecl
#endif
increment_pop_count(void * arg)
{
	sharedInt_t * sI = (sharedInt_t*)arg;

	EnterCriticalSection(&sI->cs);
	sI->i++;
	LeaveCriticalSection(&sI->cs);
}

static void * mythread(void * arg)
{
	int result = 0;
	bag_t * bag = static_cast<bag_t *>(arg);

	assert(bag == &threadbag[bag->threadnum]);
	assert(bag->started == 0);
	bag->started = 1;

	/* Set to known state and type */

	assert(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) == 0);

	assert(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) == 0);

#ifdef _MSC_VER
#pragma inline_depth(0)
#endif
	pthread_cleanup_push(increment_pop_count, (void*)&pop_count);
	/*
	 * We don't have true async cancellation - it relies on the thread
	 * at least re-entering the run state at some point.
	 * We wait up to 10 seconds, waking every 0.1 seconds,
	 * for a cancellation to be applied to us.
	 */
	for(bag->count = 0; bag->count < 100; bag->count++)
		Sleep(100);

	pthread_cleanup_pop(0);
#ifdef _MSC_VER
#pragma inline_depth()
#endif

	return (void*)(size_t)result;
}

int main()
{
	int failed = 0;
	int i;
	pthread_t t[NUMTHREADS + 1];

	DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
	SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);

	memset(&pop_count, 0, sizeof(sharedInt_t));

	InitializeCriticalSection(&pop_count.cs);

	assert((t[0] = pthread_self()).p != NULL);

	for(i = 1; i <= NUMTHREADS; i++) {
		threadbag[i].started = 0;
		threadbag[i].threadnum = i;
		assert(pthread_create(&t[i], NULL, mythread, (void*)&threadbag[i]) == 0);
	}
	/*
	 * Code to control or manipulate child threads should probably go here.
	 */
	Sleep(500);
	for(i = 1; i <= NUMTHREADS; i++) {
		assert(pthread_cancel(t[i]) == 0);
	}
	Sleep(NUMTHREADS * 100); // Give threads time to run.
	// Standard check that all threads started.
	for(i = 1; i <= NUMTHREADS; i++) {
		if(!threadbag[i].started) {
			failed |= !threadbag[i].started;
			fprintf(stderr, "Thread %d: started %d\n", i, threadbag[i].started);
		}
	}
	assert(!failed);
	// Check any results here. Set "failed" and only print output on failure.
	failed = 0;
	for(i = 1; i <= NUMTHREADS; i++) {
		int fail = 0;
		void* result = (void*)0;
		assert(pthread_join(t[i], &result) == 0);
		fail = (result != PTHREAD_CANCELED);
		if(fail) {
			fprintf(stderr, "Thread %d: started %d: result %d\n", i, threadbag[i].started, (int)(size_t)result);
		}
		failed = (failed || fail);
	}
	assert(!failed);
	assert(pop_count.i == NUMTHREADS);
	DeleteCriticalSection(&pop_count.cs);
	return 0; // Success
}

#else /* defined(_MSC_VER) || defined(__cplusplus) */

int main()
{
	return 0;
}

#endif /* defined(_MSC_VER) || defined(__cplusplus) */
