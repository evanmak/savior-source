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

#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>


#ifdef __USE_MISC
#undef __USE_MISC
#define __REDEF_MISC
#endif

#ifdef __USE_XOPEN2K8
#undef __USE_XOPEN2K8
#define __REDEF_XOPEN2K8
#endif

#include <sys/stat.h>

#ifdef __REDEF_MISC
#define __USE_MISC 1
#undef __REDEF_MISC
#endif

#ifdef __REDEF_XOPEN2K8
#define __USE_XOPEN2K8 1
#undef __REDEF_XOPEN2K8
#endif

#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// IOCtl Codes
////////////////////////////////////////////////////////////////////////////////

#define KLEE_SIO_SYMREADS   0xfff00     // Enable symbolic reads for a socket
#define KLEE_SIO_READCAP    0xfff01     // Set a maximum cap for reading from the stream

////////////////////////////////////////////////////////////////////////////////
// Klee Event Codes
////////////////////////////////////////////////////////////////////////////////

#define __KLEE_EVENT_BREAKPOINT   10
#define __KLEE_EVENT_PKT_FRAGMENT   1337

////////////////////////////////////////////////////////////////////////////////
// Klee Breakpoint Types
////////////////////////////////////////////////////////////////////////////////

#define __KLEE_BREAK_TRACE  42

////////////////////////////////////////////////////////////////////////////////
// Klee Fork Types
////////////////////////////////////////////////////////////////////////////////

#define __KLEE_FORK_DEFAULT       0
#define __KLEE_FORK_FAULTINJ      1


////////////////////////////////////////////////////////////////////////////////
// Model Infrastructure
////////////////////////////////////////////////////////////////////////////////

// A model needs to be declared only when it's supposed to interface
// an existing C library call.

#define __DECLARE_MODEL(type, name, ...) \
  type __klee_model_ ## name(__VA_ARGS__); \
  type __klee_original_ ## name(__VA_ARGS__);

#ifdef __FORCE_USE_MODELS
#define DECLARE_MODEL(type, name, ...) \
  __DECLARE_MODEL(type, name, ##__VA_ARGS__) \
  __attribute__((used)) static const void* __usage_model_ ## name = (void*)&__klee_model_ ## name;
#else
#define DECLARE_MODEL(type, name, ...) \
  __DECLARE_MODEL(type, name, ##__VA_ARGS__)
#endif

#define FORCE_IMPORT(name) \
  __attribute__((used)) static const void* __usage_ ## name = (void*) &name;

// Peng begin
/* #define CALL_UNDERLYING(name, ...) \ */
/*     __klee_original_ ## name(__VA_ARGS__) */
#define CALL_UNDERLYING(name, ...)		\
  name(__VA_ARGS__)
// Peng end

#define CALL_MODEL(name, ...) \
    __klee_model_ ## name(__VA_ARGS__);

// Peng begin
#define DEFINE_MODEL(type, name, ...) \
    type __klee_model_ ## name(__VA_ARGS__)
/* #define DEFINE_MODEL(type, name, ...) \ */
/*     type name(__VA_ARGS__) */
// Peng end

#ifdef HAVE_FAULT_INJECTION

#define INJECT_FAULT(name, ...) \
    __inject_fault(#name, ##__VA_ARGS__)

#else

#define INJECT_FAULT(name, ...)     0

#endif

int klee_get_errno(void);

void *__concretize_ptr(const void *p);
size_t __concretize_size(size_t s);
const char *__concretize_string(const char *s);

unsigned __fork_values(unsigned min, unsigned max, int reason);

int __inject_fault(const char *fname, int errno, ...);

////////////////////////////////////////////////////////////////////////////////
// Basic Arrays
////////////////////////////////////////////////////////////////////////////////

// XXX Remove this, since it's unused?

#define ARRAY_INIT(arr) \
  do { memset(&arr, 0, sizeof(arr)); } while (0)

#define ARRAY_ALLOC(arr, item) \
  do { \
    item = sizeof(arr)/sizeof(arr[0]); \
    unsigned int __i; \
    for (__i = 0; __i < sizeof(arr)/sizeof(arr[0]); __i++) { \
      if (!arr[__i]) { \
        item = __i; break; \
      } \
    } \
  } while (0)

#define ARRAY_CLEAR(arr, item) \
  do { arr[item] = 0; } while (0)

#define ARRAY_CHECK(arr, item) \
  ((item < sizeof(arr)/sizeof(arr[0])) && arr[item] != 0)


////////////////////////////////////////////////////////////////////////////////
// Static Lists
////////////////////////////////////////////////////////////////////////////////

#define STATIC_LIST_INIT(list)  \
  do { memset(&list, 0, sizeof(list)); } while (0)

#define STATIC_LIST_ALLOC(list, item) \
  do { \
    item = sizeof(list)/sizeof(list[0]); \
    unsigned int __i; \
    for (__i = 1; __i < sizeof(list)/sizeof(list[0]); __i++) { \
      if (!list[__i].allocated) { \
        list[__i].allocated = 1; \
        item = __i;  break; \
      } \
    } \
  } while (0)

#define STATIC_LIST_CLEAR(list, item) \
  do { memset(&list[item], 0, sizeof(list[item])); } while (0)

#define STATIC_LIST_CHECK(list, item) \
  (((item) < sizeof(list)/sizeof(list[0])) && (list[item].allocated))

////////////////////////////////////////////////////////////////////////////////
// Double Linked Lists (Linux kernel style)
////////////////////////////////////////////////////////////////////////////////
struct list_head {
  struct list_head *next, *prev;
};

#define list_entry(ptr, type, member) \
  ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define list_for_each_entry(pos, head, member)                    \
  for (pos = list_entry((head)->next, typeof(*pos), member);      \
       &pos->member != (head);                                    \
       pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_del(l_prev, l_next) \
  do { \
    (l_next)->prev = (struct list_head *)l_prev; \
    (l_prev)->next = (struct list_head *)l_next;  \
  } while(0)

#define list_del_init(entry) 			\
  do { 						\
    list_del(((struct list_head *)entry)->prev, \
      ((struct list_head *)entry)->next);	\
    INIT_LIST_HEAD(entry);			\
  } while(0)

#define INIT_LIST_HEAD(list) \
  do { \
    ((struct list_head *)list)->next = (list); \
    ((struct list_head *)list)->prev = (list); \
  } while(0)

////////////////////////////////////////////////////////////////////////////////
// IOvec manipulation
////////////////////////////////////////////////////////////////////////////////

static inline size_t __count_iovec(const struct iovec *iov, int iovcnt) {
  size_t result = 0;
  int i;
  for (i = 0; i < iovcnt; i++)
    result += iov[i].iov_len;
  return result;
}

#endif /* COMMON_H_ */
