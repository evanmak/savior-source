/*
 * Cloud9 Parallel Symbolic Execution Engine
 *
 * Copyright (c) 2011, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in CLOUD9-AUTHORS file.
 *
 */

#include "multiprocess.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// IPC Semaphores
////////////////////////////////////////////////////////////////////////////////

// Taken from the manual page
union semun {
   int              val;    /* Value for SETVAL */
   struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
   unsigned short  *array;  /* Array for GETALL, SETALL */
   struct seminfo  *__buf;  /* Buffer for IPC_INFO
                               (Linux-specific) */
};

int semget(key_t key, int nsems, int semflg) {
  if (nsems < 0) {
    errno = EINVAL;
    return -1;
  }

  int index = MAX_SEMAPHORES;
  if (key != IPC_PRIVATE) {
    // Search for the key, if the set is already created
    for (index = 0; index < MAX_SEMAPHORES; index++) {
      if (STATIC_LIST_CHECK(__sems, (unsigned)index) &&
          __sems[index].descriptor.sem_perm.__key == key) {
        // This is it
        break;
      }
    }
  }

  if (index == MAX_SEMAPHORES) {
    if (!(semflg & IPC_CREAT)) {
      errno = ENOENT;
      return -1;
    }

    if (nsems == 0) {
      errno = EINVAL;
      return -1;
    }

    // Try to allocate a new semaphore set
    STATIC_LIST_ALLOC(__sems, index);

    if (index == MAX_SEMAPHORES) {
      errno = ENOSPC;
      return -1;
    }

    // At this point we know the index of the semaphore set
    sem_set_t *semset = &__sems[index];

    semset->sems = (sem_t*)malloc(nsems * sizeof(sem_t));
    memset(semset->sems, 0, nsems * sizeof(sem_t));
    klee_make_shared(semset->sems, nsems * sizeof(sem_t));

    semset->descriptor.sem_nsems = nsems;
    semset->descriptor.sem_ctime = time(0);
    semset->descriptor.sem_otime = 0;

    struct ipc_perm *perm = &semset->descriptor.sem_perm;
    perm->__key = key;
    perm->__seq = 0;
    perm->mode = semflg & 0777;
  } else {
    if ((semflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
      errno = EEXIST;
      return -1;
    }
  }

  return index;
}

static void _sem_delete(int semid) {
  sem_set_t *semset = &__sems[semid];
  free(semset->sems);

  STATIC_LIST_CLEAR(__sems, semid);
}

static void _sem_setval(int semid, int semnum, unsigned short val) {
  sem_set_t *semset = &__sems[semid];

  semset->sems[semnum].semval = val;

  semset->descriptor.sem_ctime = time(0);
}

static void _sem_setall(int semid, unsigned short *vals) {
  sem_set_t *semset = &__sems[semid];

  unsigned i;
  for (i = 0; i < semset->descriptor.sem_nsems; i++) {
    semset->sems[i].semval = vals[i];
  }

  semset->descriptor.sem_ctime = time(0);
}

int semctl(int semid, int semnum, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__sems, (unsigned)semid)) {
    errno = EINVAL;
    return -1;
  }

  union semun arg;
  va_list ap;

  // Get the argument
  va_start(ap, cmd);
  arg = va_arg(ap, union semun);
  va_end(ap);

  switch (cmd) {
  case IPC_RMID:
    _sem_delete(semid);
    return 0;
  case SETALL:
    _sem_setall(semid, arg.array);
    return 0;
  case SETVAL:
    _sem_setval(semid, semnum, arg.val);
    return 0;

  case IPC_STAT:
  case IPC_SET:
  case IPC_INFO:
  case SEM_INFO:
  case SEM_STAT:
  case GETALL:
  case GETNCNT:
  case GETPID:
  case GETVAL:
  case GETZCNT:
    klee_warning("unsupported semaphore operation");
    errno = EINVAL;
    return -1;
  default:
    // Invalid semaphore command
    errno = EINVAL;
    return -1;
  }
}

int semop(int semid, struct sembuf *sops, size_t nsops) {
  assert(0 && "not implemented");
}

int semtimedop(int semid, struct sembuf *sops, size_t nsops,
    const struct timespec *timeout) {
  assert(0 && "not implemented");
}

////////////////////////////////////////////////////////////////////////////////
// POSIX Semaphores
////////////////////////////////////////////////////////////////////////////////
#define SEM_VALUE_MAX 32000
# define __SIZEOF_SEM_T	32
typedef union
{
  char __size[__SIZEOF_SEM_T];
  long int __align;
} sem_posix_t;

static void _sem_init(sem_posix_t *sem, int pshared, unsigned int value) {
  sem_data_t *sdata = (sem_data_t*)malloc(sizeof(sem_data_t));
  memset(sdata, 0, sizeof(sem_data_t));

  *((sem_data_t**)sem) = sdata;

  sdata->wlist = klee_get_wlist();
  sdata->count = value;
	if(!pshared) {
		sdata->thread_level = 1;
		sdata->owner = getpid();
	}
}

static sem_data_t *_get_sem_data(sem_posix_t *sem) {
  sem_data_t *sdata = *((sem_data_t**)sem);

  return sdata;
}

int sem_init(sem_posix_t *sem, int pshared, unsigned int value) {
	if(value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}
	
	_sem_init(sem, pshared, value);

	return 0;
}

int sem_destroy(sem_posix_t *sem) {
  sem_data_t *sdata = _get_sem_data(sem);

  free(sdata);

  return 0;
}

static int _atomic_sem_lock(sem_data_t *sdata, char try) {
	if(sdata->thread_level  &&  sdata->owner != getpid()) {
		errno = EINVAL;
		return -1;
	}

	sdata->count--;
  if (sdata->count < 0) {
    if (try) {
			sdata->count++;
      errno = EBUSY;
      return -1;
    } else {
      __thread_sleep(sdata->wlist);
    }
  }

  return 0;
}

int sem_wait(sem_posix_t *sem) {
  sem_data_t *sdata = _get_sem_data(sem);

  int res = _atomic_sem_lock(sdata, 0);

  if (res == 0)
    __thread_preempt(0);

  return res;
}

int sem_trywait(sem_posix_t *sem) {
  sem_data_t *sdata = _get_sem_data(sem);

  int res = _atomic_sem_lock(sdata, 1);

  if (res == 0)
    __thread_preempt(0);

  return res;
}

static int _atomic_sem_unlock(sem_data_t *sdata) {
	if(sdata->thread_level  &&  sdata->owner != getpid()) {
		errno = EINVAL;
		return -1;
	}

  sdata->count++;

  if (sdata->count <= 0)
    __thread_notify_one(sdata->wlist);

  return 0;
}

int sem_post(sem_posix_t *sem) {
  sem_data_t *sdata = _get_sem_data(sem);

  int res = _atomic_sem_unlock(sdata);

  __thread_preempt(0);

  return res;
}
