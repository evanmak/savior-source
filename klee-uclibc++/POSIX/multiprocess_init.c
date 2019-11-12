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
#include "signals.h"

#include "models.h"
#include "common.h"

#include <string.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Processes
////////////////////////////////////////////////////////////////////////////////

proc_data_t __pdata[MAX_PROCESSES];
sem_set_t __sems[MAX_SEMAPHORES];

static void klee_init_semaphores(void) {
  STATIC_LIST_INIT(__sems);
  klee_make_shared(__sems, sizeof(__sems));
}

void klee_init_processes(void) {
  STATIC_LIST_INIT(__pdata);
  klee_make_shared(__pdata, sizeof(__pdata));

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(DEFAULT_PROCESS)];
  pdata->allocated = 1;
  pdata->terminated = 0;
  pdata->parent = DEFAULT_PARENT;
  pdata->umask = DEFAULT_UMASK;
  pdata->wlist = klee_get_wlist();
  pdata->children_wlist = klee_get_wlist();

  klee_init_semaphores();

  klee_init_threads();

#ifdef HAVE_POSIX_SIGNALS
  klee_init_signals();
#endif

}

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////

tsync_data_t __tsync;

void klee_init_threads(void) {
  STATIC_LIST_INIT(__tsync.threads);

  // Thread initialization
  thread_data_t *def_data = &__tsync.threads[DEFAULT_THREAD];
  def_data->allocated = 1;
  def_data->terminated = 0;
  def_data->ret_value = 0;
  def_data->joinable = 1; // Why not?
  def_data->wlist = klee_get_wlist();
}
