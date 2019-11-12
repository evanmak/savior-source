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

#include "pipes.h"

#include "fd.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Internal routines
////////////////////////////////////////////////////////////////////////////////

int _close_pipe(pipe_end_t *pipe) {
  if (!_stream_is_closed(pipe->buffer)) {
    _stream_close(pipe->buffer);
  } else {
    _stream_destroy(pipe->buffer);
  }

  pipe->buffer = NULL;

  if (pipe->__bdata.queued == 0) {
    free(pipe);
  }

  return 0;
}

ssize_t _read_pipe(pipe_end_t *pipe, void *buf, size_t count) {
  if (count == 0)
    return 0;

  if (pipe->__bdata.flags & O_NONBLOCK) {
    if (_stream_is_empty(pipe->buffer) && !_stream_is_closed(pipe->buffer)) {

      errno = EAGAIN;
      return -1;
    }
  }

  pipe->__bdata.queued++;
  ssize_t res = _stream_read(pipe->buffer, buf, count);
  pipe->__bdata.queued--;

  if (pipe->buffer == NULL) {
    if (pipe->__bdata.queued == 0)
      free(pipe);

    errno = EINVAL;
    return -1;
  }

  if (res == -1) {
    errno = EINVAL;
  }

  return res;
}

ssize_t _write_pipe(pipe_end_t *pipe, const void *buf, size_t count) {
  if (_stream_is_closed(pipe->buffer)) {
    errno = EPIPE;
    return -1;
  }

  if (count == 0)
    return 0;

  if (pipe->__bdata.flags & O_NONBLOCK) {
    if (_stream_is_full(pipe->buffer) && !_stream_is_closed(pipe->buffer)) {
      errno = EAGAIN;
      return -1;
    }
  }

  pipe->__bdata.queued++;
  ssize_t res = _stream_write(pipe->buffer, buf, count);
  pipe->__bdata.queued--;

  if (pipe->buffer == NULL) {
    if (pipe->__bdata.queued == 0)
      free(pipe);

    errno = EINVAL;
    return -1;
  }

  if (res == 0) {
    errno = EPIPE;
  }

  if (res == -1) {
    errno = EINVAL;
  }

  return res;
}

int _stat_pipe(pipe_end_t *pipe, struct stat *buf) {
  assert(0 && "not implemented");
}

////////////////////////////////////////////////////////////////////////////////

int _is_blocking_pipe(pipe_end_t *pipe, int event) {
  switch (event) {
  case EVENT_READ:
    return _stream_is_empty(pipe->buffer) && !_stream_is_closed(pipe->buffer);
  case EVENT_WRITE:
    return _stream_is_full(pipe->buffer) && !_stream_is_closed(pipe->buffer);
  default:
    assert(0 && "invalid event");
  }
}

int _register_events_pipe(pipe_end_t *pipe, wlist_id_t wlist, int events) {
  return _stream_register_event(pipe->buffer, wlist);
}

void _deregister_events_pipe(pipe_end_t *pipe, wlist_id_t wlist, int events) {
  _stream_clear_event(pipe->buffer, wlist);
}

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////

int pipe(int pipefd[2]) {
  int fdr, fdw;

  klee_debug("Attempting to create a pipe\n");

  // Allocate the two file descriptors
  STATIC_LIST_ALLOC(__fdt, fdr);

  if (fdr == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  STATIC_LIST_ALLOC(__fdt, fdw);

  if (fdw == MAX_FDS) {
    STATIC_LIST_CLEAR(__fdt, fdr);

    errno = ENFILE;
    return -1;
  }

  // Initialize the stream buffer
  stream_buffer_t *buff = _stream_create(PIPE_BUFFER_SIZE, 1);

  // Create the pipe read point
  pipe_end_t *piper = (pipe_end_t*)malloc(sizeof(pipe_end_t));
  klee_make_shared(piper, sizeof(pipe_end_t));
  memset(piper, 0, sizeof(pipe_end_t));

  piper->__bdata.flags |= O_RDONLY;
  piper->__bdata.refcount = 1;
  piper->__bdata.queued = 0;
  piper->buffer = buff;

  __fdt[fdr].attr |= FD_IS_PIPE;
  __fdt[fdr].io_object = (file_base_t*)piper;

  // Create the pipe write point
  pipe_end_t *pipew = (pipe_end_t*)malloc(sizeof(pipe_end_t));
  klee_make_shared(pipew, sizeof(pipe_end_t));
  memset(pipew, 0, sizeof(pipe_end_t));

  pipew->__bdata.flags |= O_WRONLY;
  pipew->__bdata.refcount = 1;
  pipew->__bdata.queued = 0;
  pipew->buffer = buff;

  __fdt[fdw].attr |= FD_IS_PIPE;
  __fdt[fdw].io_object = (file_base_t*)pipew;

  pipefd[0] = fdr; pipefd[1] = fdw;

  klee_debug("Pipe created (%d, %d)\n", fdr, fdw);
  return 0;
}
