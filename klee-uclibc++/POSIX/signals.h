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

#ifndef SIGNALS_H_
#define SIGNALS_H_

#include "common.h"

#include <signal.h>
#include <stdint.h>

#ifdef HAVE_POSIX_SIGNALS

/*
 * This is architecture dependent. We should do this
 * from autoconf or something similar.
 */
#define BITS_PER_LONG 	64
#define _NSIG_WORDS	_SIGSET_NWORDS
#define _NSIG_BPW 	32
#define SA_RESTORER 	0x04000000

#define siginmask(sig, mask) (sigmask(sig) & (mask))

#ifdef SIGEMT
#define SIGEMT_MASK 	sigmask(SIGEMT)
#else
#define SIGEMT_MASK 	0
#endif

#define SIG_STOP_MASK (\
  sigmask(SIGSTOP)   |  sigmask(SIGTSTP)   | \
  sigmask(SIGTTIN)   |  sigmask(SIGTTOU)   )

#define SIG_COREDUMP_MASK (\
  sigmask(SIGQUIT)   |  sigmask(SIGILL)    | \
  sigmask(SIGTRAP)   |  sigmask(SIGABRT)   | \
  sigmask(SIGFPE)    |  sigmask(SIGSEGV)   | \
  sigmask(SIGBUS)    |  sigmask(SIGSYS)    | \
  sigmask(SIGXCPU)   |  sigmask(SIGXFSZ)   | \
  SIGEMT_MASK                                    )

#define SIG_IGNORE_MASK (\
  sigmask(SIGCONT)   |  sigmask(SIGCHLD)   | \
  sigmask(SIGWINCH)  |  sigmask(SIGURG)    )

#define sig_coredump(sig) \
  (((sig) < SIGRTMIN) && siginmask(sig, SIG_COREDUMP_MASK))
#define sig_ignore(sig) \
  (((sig) < SIGRTMIN) && siginmask(sig, SIG_IGNORE_MASK))
#define sig_stop(sig) \
  (((sig) < SIGRTMIN) && siginmask(sig, SIG_STOP_MASK))

/*
 * These are the signals that can be executed
 * synchronously (waiting for them).
 * They are important because they are
 * usually run first.
 */
#define SYNCHRONOUS_MASK \
         (sigmask(SIGSEGV) | sigmask(SIGBUS) | sigmask(SIGILL) | \
          sigmask(SIGTRAP) | sigmask(SIGFPE))

#define valid_signal(x) ( ((x) <= _NSIG ? 1 : 0) && (x) >= 1 )

#define _SIG_SET_BINOP(name, op)						\
static inline void name(sigset_t *r, const sigset_t *a, const sigset_t *b)	\
{										\
  unsigned long a0, a1, a2, a3, b0, b1, b2, b3;					\
										\
  switch (_NSIG_WORDS) {							\
    case 4:									\
      a3 = a->__val[3]; a2 = a->__val[2];					\
      b3 = b->__val[3]; b2 = b->__val[2];					\
      r->__val[3] = op(a3, b3);							\
      r->__val[2] = op(a2, b2);							\
    case 2:									\
      a1 = a->__val[1]; b1 = b->__val[1];					\
      r->__val[1] = op(a1, b1);							\
    case 1:									\
      a0 = a->__val[0]; b0 = b->__val[0];					\
      r->__val[0] = op(a0, b0);							\
      break;									\
  }                                                               		\
}

#define _sig_or(x,y)	((x) | (y))
_SIG_SET_BINOP(sigorsets, _sig_or)

static inline void sigdelsetmask(__sigset_t *set, unsigned long mask) {
  set->__val[0] &= ~mask;
}

static inline int has_pending_signals(sigset_t *signal, sigset_t *blocked) {
  unsigned long ready;
  long i;

  switch (_NSIG_WORDS) {
    default:
      for (i = _NSIG_WORDS, ready = 0; --i >= 0 ;)
      ready |= signal->__val[i] &~ blocked->__val[i];
      break;

    case 4: ready  = signal->__val[3] &~ blocked->__val[3];
      ready |= signal->__val[2] &~ blocked->__val[2];
      ready |= signal->__val[1] &~ blocked->__val[1];
      ready |= signal->__val[0] &~ blocked->__val[0];
      break;

    case 2: ready  = signal->__val[1] &~ blocked->__val[1];
      ready |= signal->__val[0] &~ blocked->__val[0];
      break;

    case 1: ready  = signal->__val[0] &~ blocked->__val[0];
  }
  return ready != 0;
}

struct sigpending {
  struct list_head list; /* List of sigqueue */
  sigset_t signal;
};

struct sigqueue {
  struct list_head list;
  int flags;
  siginfo_t info;
  /* TODO: Should we include user cred info here? */
};

typedef void (*sighandler_t)(int);
typedef void (*sigrestore_t)(void);
struct k_sigaction {
  sighandler_t k_sa_handler;
  unsigned long sa_flags;
  sigrestore_t sa_restorer;
  sigset_t sa_mask;
};

struct sighand_struct {
  int count;
  struct k_sigaction action[_NSIG];
  /*
   * We should add support for signalfd waiting queues here.
   */
};

void __handle_signal();
void klee_init_signals();

int __syscall_rt_sigaction(int signum, const struct k_sigaction *act,
                           struct k_sigaction *oldact, size_t _something)
     __attribute__((weak));

int sigaction(int signum, const struct sigaction *act, 
              struct sigaction *oldact) __attribute__((weak));

sighandler_t signal(int signum, sighandler_t handler);
sighandler_t sigset(int signum, sighandler_t disp);
int kill(int pid, int signum);
int sighold(int signum);
int sigrelse(int signum);
int sigignore(int signum);
unsigned int alarm(unsigned int seconds);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
     __attribute__((weak));

#endif /* SIGNALS_H_ */

#endif /* HAVE_POSIX_SIGNALS */
