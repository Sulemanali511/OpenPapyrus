/*
 * rwlock2.c
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
 * Declare a static rwlock object, lock it,
 * and then unlock it again.
 *
 * Depends on API functions:
 *	pthread_rwlock_rdlock()
 *	pthread_rwlock_unlock()
 */

#include "test.h"

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

int main()
{
	assert(rwlock == PTHREAD_RWLOCK_INITIALIZER);
	assert(pthread_rwlock_rdlock(&rwlock) == 0);
	assert(rwlock != PTHREAD_RWLOCK_INITIALIZER);
	assert(rwlock != NULL);
	assert(pthread_rwlock_unlock(&rwlock) == 0);
	assert(pthread_rwlock_destroy(&rwlock) == 0);
	assert(rwlock == NULL);
	return 0;
}
