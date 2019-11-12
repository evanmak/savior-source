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
#include "fd.h"
#include "signals.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <klee/klee.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////

pid_t getpid(void) {
  pid_t pid;
  klee_get_context(0, &pid);

  return pid;
}

pid_t getppid(void) {
  pid_t pid;
  klee_get_context(0, &pid);

  return __pdata[PID_TO_INDEX(pid)].parent;
}

mode_t umask(mode_t mask) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  mode_t res = pdata->umask;

  pdata->umask = mask & 0777;

  return res;
}

void _exit(int status) {
  pid_t pid = getpid();

  // Closing all open file descriptors
  __close_fds();

  // Checking for zombie processes
  unsigned int idx;
  for (idx = 0; idx < MAX_PROCESSES; idx++) {
    if (__pdata[idx].allocated && __pdata[idx].parent == pid) {
      // Reassign the child to process "init"
      __pdata[idx].parent = DEFAULT_PARENT;
    }
  }

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(pid)];
  pdata->terminated = 1;
  pdata->ret_value = status;
  __thread_notify_all(pdata->wlist);

  if (pdata->parent != DEFAULT_PARENT) {
    proc_data_t *ppdata = &__pdata[PID_TO_INDEX(pdata->parent)];
    __thread_notify_all(ppdata->children_wlist);
  }

  klee_process_terminate();
}

pid_t fork(void) {
  unsigned int newIdx;
  STATIC_LIST_ALLOC(__pdata, newIdx);

  if (newIdx == MAX_PROCESSES) {
    errno = ENOMEM;
    return -1;
  }

  if (INJECT_FAULT(fork, EAGAIN, ENOMEM)) {
    return -1;
  }

  proc_data_t *ppdata = &__pdata[PID_TO_INDEX(getpid())];

  proc_data_t *pdata = &__pdata[newIdx];
  pdata->terminated = 0;
  pdata->wlist = klee_get_wlist();
  pdata->children_wlist = klee_get_wlist();
  pdata->parent = getpid();
  pdata->umask = ppdata->umask;

#ifdef HAVE_POSIX_SIGNALS
  /* Clone necessary signal info.*/
  pdata->signaled = 0;
  INIT_LIST_HEAD(&pdata->pending.list);
  sigemptyset(&pdata->blocked);
  pdata->sighand = calloc(1, sizeof(struct sighand_struct));
  memcpy(&pdata->sighand->action, &ppdata->sighand->action, sizeof(pdata->sighand->action));
#endif

  fd_entry_t shadow_fdt[MAX_FDS];

  // We need this hack in order to make the fork and FDT duplication atomic.
  memcpy(shadow_fdt, __fdt, sizeof(shadow_fdt));
  __adjust_fds_on_fork();

  int res = klee_process_fork(INDEX_TO_PID(newIdx)); // Here we split our ways...

  if (res == 0) {
    // We're in the child. Re-initialize the threading structures
    klee_init_threads();
#ifdef HAVE_POSIX_SIGNALS
    /*
     * The child is always scheduled after the parent.
     * During this time it may have been signaled.
     */
    if(pdata->signaled)
      __handle_signal();
#endif
  } else {
    memcpy(__fdt, shadow_fdt, sizeof(__fdt));
  }

  return res;
}

pid_t vfork(void) {
  pid_t pid = fork();

  if (pid > 0) {
    waitpid(pid, 0, 0);
  }

  return pid;
}

pid_t wait(int *status) {
  return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
  if (pid < -1 || pid == 0) {
    klee_warning("unsupported process group wait() call");

    errno = EINVAL;
    return -1;
  }

  if ((WUNTRACED | WCONTINUED) & options) {
    klee_warning("unsupported waitpid() options");

    errno = EINVAL;
    return -1;
  }

  pid_t ppid = getpid();

  unsigned int idx = MAX_PROCESSES; // The index of the terminated child
  proc_data_t *pdata = 0;

  if (pid == -1) {
    do {
      // Look up children
      int i;
      int hasChildren = 0;

      for (i = 0; i < MAX_PROCESSES; i++) {
        if (!__pdata[i].allocated || __pdata[i].parent != ppid)
          continue;

        hasChildren = 1;
        if (__pdata[i].terminated) {
          idx = i;
          pdata = &__pdata[i];
          break;
        }
      }

      if (idx != MAX_PROCESSES)
        break;

      if (!hasChildren) {
        errno = ECHILD;
        return -1;
      }

      if (WNOHANG & options)
        return 0;

      __thread_sleep(__pdata[PID_TO_INDEX(ppid)].children_wlist);

    } while (1);

  } else {
    idx = PID_TO_INDEX(pid);

    if (idx >= MAX_PROCESSES) {
      errno = ECHILD;
      return -1;
    }

    pdata = &__pdata[idx];

    if (!pdata->allocated || pdata->parent != ppid) {
      errno = ECHILD;
      return -1;
    }

    if (!pdata->terminated) {
      if (WNOHANG & options)
        return 0;

      __thread_sleep(pdata->wlist);
    }
  }

  if (status) {
    // XXX Is this documented stuff?
    *status = (pdata->ret_value & 0xff) << 8;
  }

  // Now we can safely clean up the process structure
  STATIC_LIST_CLEAR(__pdata, idx);

  return INDEX_TO_PID(idx);
}

int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options) {
  assert(0 && "not implemented");
}
