/*
 * spin1.c
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
 * Create a simple spinlock object, lock it, and then unlock it again.
 * This is the simplest test of the pthread mutex family that we can do.
 *
 */
#include "test.h"

pthread_spinlock_t lock;

int main()
{
	assert(pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE) == 0);
	assert(pthread_spin_lock(&lock) == 0);
	assert(pthread_spin_unlock(&lock) == 0);
	assert(pthread_spin_destroy(&lock) == 0);
	assert(pthread_spin_lock(&lock) == EINVAL);
	return 0;
}
