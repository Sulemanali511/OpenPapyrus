/*
 * File: semaphore4.c
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
 * Test Synopsis: Verify sem_getvalue returns the correct number of waiters
 * after threads are cancelled.
 * -
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
 * -
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */
#include "test.h"

#define MAX_COUNT 100

static sem_t s;

static void * thr(void * arg)
{
	assert(sem_wait(&s) == 0);
	return NULL;
}

int main()
{
	int value = 0;
	int i;
	pthread_t t[MAX_COUNT+1];

	assert(sem_init(&s, PTHREAD_PROCESS_PRIVATE, 0) == 0);
	assert(sem_getvalue(&s, &value) == 0);
	assert(value == 0);

	for(i = 1; i <= MAX_COUNT; i++) {
		assert(pthread_create(&t[i], NULL, thr, NULL) == 0);
		do {
			sched_yield();
			assert(sem_getvalue(&s, &value) == 0);
		} while(value != -i);
		assert(-value == i);
	}
	assert(sem_getvalue(&s, &value) == 0);
	assert(-value == MAX_COUNT);
	assert(pthread_cancel(t[50]) == 0);
	{
		void* result;
		assert(pthread_join(t[50], &result) == 0);
	}
	assert(sem_getvalue(&s, &value) == 0);
	assert(-value == (MAX_COUNT - 1));
	for(i = MAX_COUNT - 2; i >= 0; i--) {
		assert(sem_post(&s) == 0);
		assert(sem_getvalue(&s, &value) == 0);
		assert(-value == i);
	}
	for(i = 1; i <= MAX_COUNT; i++)
		if(i != 50)
			assert(pthread_join(t[i], NULL) == 0);
	return 0;
}
