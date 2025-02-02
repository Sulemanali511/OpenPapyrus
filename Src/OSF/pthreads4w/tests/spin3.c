/*
 * spin3.c
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
 * Thread A locks spin - thread B tries to unlock.
 * This should succeed, but it's undefined behaviour.
 *
 */
#include "test.h"

static int wasHere = 0;
static pthread_spinlock_t spin;

void * unlocker(void * arg)
{
	int expectedResult = (int)(size_t)arg;
	wasHere++;
	assert(pthread_spin_unlock(&spin) == expectedResult);
	wasHere++;
	return NULL;
}

int main()
{
	pthread_t t;
	wasHere = 0;
	assert(pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) == 0);
	assert(pthread_spin_lock(&spin) == 0);
	assert(pthread_create(&t, NULL, unlocker, (void*)0) == 0);
	assert(pthread_join(t, NULL) == 0);
	/*
	 * Our spinlocks don't record the owner thread so any thread can unlock the spinlock,
	 * but nor is it an error for any thread to unlock a spinlock that is not locked.
	 */
	assert(pthread_spin_unlock(&spin) == 0);
	assert(pthread_spin_destroy(&spin) == 0);
	assert(wasHere == 2);
	return 0;
}
