/*
 * mutex3e.c
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
 * Declare a static mutex object, lock it, trylock it,
 * and then unlock it again.
 *
 * Depends on API functions:
 *	pthread_mutex_lock()
 *	pthread_mutex_trylock()
 *	pthread_mutex_unlock()
 */
#include "test.h"

pthread_mutex_t mutex1 = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;

static int washere = 0;

static void * func(void * arg)
{
	assert(pthread_mutex_trylock(&mutex1) == EBUSY);
	washere = 1;
	return 0;
}

int main()
{
	pthread_t t;
	assert(pthread_mutex_lock(&mutex1) == 0);
	assert(pthread_create(&t, NULL, func, NULL) == 0);
	assert(pthread_join(t, NULL) == 0);
	assert(pthread_mutex_unlock(&mutex1) == 0);
	assert(washere == 1);
	return 0;
}
