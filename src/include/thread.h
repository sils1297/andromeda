/*
    Orion OS, The educational operatingsystem
    Copyright (C) 2011  Bart Kuivenhoven

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>

#ifndef __THREAD_H
#define __THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define __THREAD_MUTEX_FREE 0
#define __THREAD_MUTEX_SHUT 1

#define mutex_lock mutexEnter
#define mutex_test mutexTest
#define mutex_unlock mutexRelease

#define mutex_locked 1
#define mutex_unlocked 0

typedef unsigned int mutex_t;

extern void mutexEnter(mutex_t);
extern unsigned int mutexTest(mutex_t);
extern void mutexRelease(mutex_t);

#ifdef __cplusplus
}
#endif

#endif
