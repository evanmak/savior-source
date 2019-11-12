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

#ifndef FD_H_
#define FD_H_

#include "common.h"

#include <sys/uio.h>

#define FD_IS_FILE          (1 << 3)    // The fd points to a disk file
#define FD_IS_SOCKET        (1 << 4)    // The fd points to a socket
#define FD_IS_PIPE          (1 << 5)    // The fd points to a pipe
#define FD_CLOSE_ON_EXEC    (1 << 6)    // The fd should be closed at exec() time (ignored)

typedef struct {
  unsigned int refcount;
  unsigned int queued;
  int flags;
} file_base_t;

typedef struct {
  unsigned int attr;

  file_base_t *io_object;

  char allocated;
} fd_entry_t;

extern fd_entry_t __fdt[MAX_FDS];

void klee_init_fds(unsigned n_files, unsigned file_length, char unsafe, char overlapped);

void __adjust_fds_on_fork(void);
void __close_fds(void);

#define _FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define _FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define _FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define _FD_ZERO(p)  memset((char *)(p), '\0', sizeof(*(p)))

ssize_t _scatter_read(int fd, const struct iovec *iov, int iovcnt);
ssize_t _gather_write(int fd, const struct iovec *iov, int iovcnt);

int __get_concrete_fd(int symfd);


#endif /* FD_H_ */
