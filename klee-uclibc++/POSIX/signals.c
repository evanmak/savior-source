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
#include "models.h"

#include "signals.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <bits/signum.h>
#include <signal.h>

#include <klee/klee.h>

#ifdef HAVE_POSIX_SIGNALS

////////////////////////////////////////////////////////////////////////////////
// Internal routines
////////////////////////////////////////////////////////////////////////////////
/*
 * Get the next signal to execute from the current pending signals.
 */
int next_signal(struct sigpending *pending, sigset_t *mask) {
  unsigned long i, *s, *m, x;
  int sig = 0;

  s = pending->signal.__val;
  m = mask->__val;

  /*
   * Handle the first word specially: it contains the
   * synchronous signals that need to be dequeued first.
   */
  x = *s &~ *m;
  if (x) {
    if (x & SYNCHRONOUS_MASK)
      x &= SYNCHRONOUS_MASK;
    sig = ffsll(x) + 1;
    return sig;
  }

  for (i = 1; i < _NSIG_WORDS; ++i) {
    x = *++s &~ *++m;
    if (!x)
      continue;
    sig = ffsll(x) + i*_NSIG_BPW + 1;
    break;
  }

  return sig;

}

void collect_signal(int signum, struct sigpending *list, siginfo_t *info) {

  struct sigqueue *q, *first = NULL;
  list_for_each_entry(q, &list->list, list) {
    if (q->info.si_signo == signum) {
      if (first)
        goto still_pending;
      first = q;
    }
  }

  sigdelset(&list->signal, signum);
  if (first) {
still_pending:
    list_del_init(&first->list);
    /*
     * XXX: This might not be the best way to copy this struct
     * but for the moment it should do.
     */
    memcpy(info, &first->info, sizeof(first->info));
  } else {
    /* Signal wasn't in the queue, so zero out the info. */
    info->si_signo = signum;
    info->si_errno = 0;
    info->si_code = SI_USER;
    info->si_pid = 0;
    info->si_uid = 0;
  }

}

int dequeue_signal(siginfo_t *info) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  int signum = next_signal(&pdata->pending, &pdata->blocked);

  if (signum) {
    /* TODO: Check if itimers should be reset here. */
    collect_signal(signum, &pdata->pending, info);
  }

  return signum;

}

int get_signal_to_deliver(siginfo_t *info, struct k_sigaction *return_ka) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  struct sighand_struct *sighand = pdata->sighand;
  int signum;
  for(;;) {
    struct k_sigaction *ka;
    signum = dequeue_signal(info);
    ka = &sighand->action[signum-1];

    if (ka->k_sa_handler == SIG_IGN) /* Do nothing. */
      continue;
    if (ka->k_sa_handler != SIG_DFL) {
      *return_ka = *ka;

      if (ka->sa_flags & SA_ONESHOT)
        ka->k_sa_handler = SIG_DFL;

      break;
    }

    /*
     * Getting this far it means that we are doing the
     * default thing for the signal.
     */
    if (sig_ignore(signum))
      continue;

    if (sig_stop(signum)) {
      /* TODO: Handle stopping here. */
      continue;
    }

    if (sig_coredump(signum)) {
      /* TODO: Dump core. */
    }

    /* We got here, it means we have to exit. */
    _exit(info->si_signo);

  }
  return signum;
}

/*
 * Main singal handler routine. Called when a pending signal is
 * detected.
 */
void __handle_signal() {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  struct k_sigaction ka;
  siginfo_t info;
  int signum;

  /*
   * TODO: Check thread flag status for TS_RESTORE_SIGMASK in order
   * to get the old sigset from either blocked or saved_sigmask.
   */
  signum = get_signal_to_deliver(&info, &ka);
  if (signum > 0) {
    /*
     * TODO: Here we should setup the stack frame if the handler isn't
     * run in the same stack as the process.
     */
     sigorsets(&pdata->blocked, &pdata->blocked, &ka.sa_mask);
     /*
      * In case somebody preempts themselves in a handler this enables
      * functionality for SA_NODEFER - a signal can't see itself in
      * its own handler.
      */
     if (!(ka.sa_flags & SA_NODEFER))
       sigaddset(&pdata->blocked, signum);

     if (!has_pending_signals(&pdata->pending.signal, &pdata->blocked))
       pdata->signaled = 0;

     /*
      * Now we run the user-defined handler.
      */
      ((void(*)(int))(ka.k_sa_handler))(signum);

  }

} 

void klee_init_signals() {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];

  pdata->signaled = 0;
  pdata->sighand = calloc(1, sizeof(struct sighand_struct));
  sigemptyset(&pdata->blocked);
  INIT_LIST_HEAD(&pdata->pending.list);
 
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////
int __syscall_rt_sigaction(int signum, const struct k_sigaction *act,
                           struct k_sigaction *oldact, size_t _something) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  struct k_sigaction *current_sigaction, new_sigaction;

  if (!valid_signal(signum))
    return -EINVAL;

  current_sigaction = &pdata->sighand->action[signum-1];

  /*
   * If the old sigaction pointer is not NULL,
   * replace it with the current sigaction.
   */
  if (oldact)
    *oldact = *current_sigaction;

  if (act) {
    new_sigaction = *act;
    sigdelsetmask(&new_sigaction.sa_mask, sigmask(SIGKILL) | sigmask(SIGSTOP));
    *current_sigaction = new_sigaction;
    /*
     * Add support for POSIX 3.3.1.3 here.
     */
  }

  return 0;
}

void __syscall_rt_sigreturn(void) __attribute__((weak));
void __syscall_rt_sigreturn(void) {
  /*
   * Here we should check if:
   * - there was any context saved and restore it
   * - recalculate any pending signals and decide
   *   how to execute them
   * - if the signal was run on an alternative
   *   stack switch back
   */
  klee_warning("Received rt_sigreturn syscall");
}

void __syscall_sigreturn(void)  __attribute__((weak));
void __syscall_sigreturn(void) {
  klee_warning("Received sigreturn syscall");
}



int __syscall_sigaction(int signum, const struct sigaction *act,
                           struct sigaction *oldact) {
  klee_warning("Received sigaction syscall");
  return 0;
}

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact)
	      __attribute__((weak));

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact) {
  int result;
  struct k_sigaction kact, koldact;

  if (act) {
      kact.k_sa_handler = act->sa_handler;
      memcpy (&kact.sa_mask, &act->sa_mask, sizeof (kact.sa_mask));
      kact.sa_flags = act->sa_flags;

      kact.sa_flags = act->sa_flags | SA_RESTORER;
      kact.sa_restorer = ((act->sa_flags & SA_SIGINFO)
                ? &__syscall_rt_sigreturn : &__syscall_sigreturn);
  }

  /* XXX The size argument hopefully will have to be changed to the
     real size of the user-level sigset_t.  */
  result = __syscall_rt_sigaction(signum, act ? __ptrvalue (&kact) : NULL,
          oldact ? __ptrvalue (&koldact) : NULL, _NSIG / 8);
  if (oldact && result >= 0) {
      oldact->sa_handler = koldact.k_sa_handler;
      memcpy (&oldact->sa_mask, &koldact.sa_mask, sizeof (oldact->sa_mask));
      oldact->sa_flags = koldact.sa_flags;
      oldact->sa_restorer = koldact.sa_restorer;
  }
  return result;
}

int kill(int pid, int signum) {

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(pid)];

  if (!pdata->signaled)
    pdata->signaled = 1;

  sigaddset(&pdata->pending.signal, signum-1);

  return 0;
}

sighandler_t signal(int signum, sighandler_t handler) {
  return 0;
}

sighandler_t sigset(int sig, sighandler_t disp) {
  return 0;
}

int sighold(int sig) {
  return 0;
}

int sigrelse(int sig) {
  return 0;
}

int sigignore(int sig) {
  return 0;
}

unsigned int alarm(unsigned int seconds) {
  return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  return 0;
}

#endif /* HAVE_POSIX_SIGNALS */
