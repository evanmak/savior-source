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

#include "fd.h"

#include "common.h"
#include "models.h"
#include "files.h"
#include "sockets.h"
#include "pipes.h"
#include "buffers.h"
#include "signals.h"
#include "misc.h"

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <klee/klee.h>

#ifdef _FNC
#include "fnc_redhat.h"
#endif

// For multiplexing multiple syscalls into a single _read/_write op. set
#define _IO_TYPE_SCATTER_GATHER  0x1
#define _IO_TYPE_POSITIONAL      0x2

////////////////////////////////////////////////////////////////////////////////
// Internal routines
////////////////////////////////////////////////////////////////////////////////

void __adjust_fds_on_fork(void) {
  // Increment the ref. counters for all the open files pointed by the FDT.
  int fd;
  for (fd = 0; fd < MAX_FDS; fd++) {
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd))
      continue;

    __fdt[fd].io_object->refcount++;
  }
}

void __close_fds(void) {
  int fd;
  for (fd = 0; fd < MAX_FDS; fd++) {
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd))
      continue;

    close(fd);
  }
}

int __get_concrete_fd(int symfd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)symfd)) {
    errno = EBADFD;
    return -1;
  }

  fd_entry_t *fde = &__fdt[symfd];

  if (!(fde->attr & FD_IS_FILE)) {
    errno = EINVAL;
    return -1;
  }

  file_t *file = (file_t*)fde->io_object;

  if (file->concrete_fd < 0) {
    errno = EINVAL;
    return -1;
  }

  return file->concrete_fd;
}

static int _is_blocking(int fd, int event) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    return 0;
  }

  switch (event) {
  case EVENT_READ:
    if ((__fdt[fd].io_object->flags & O_ACCMODE) == O_WRONLY) {
      return 0;
    }
    break;
  case EVENT_WRITE:
    if ((__fdt[fd].io_object->flags & O_ACCMODE) == O_RDONLY) {
      return 0;
    }
    break;
  default:
    assert(0 && "invalid event");
  }

  if (__fdt[fd].attr & FD_IS_FILE) {
    return _is_blocking_file((file_t*)__fdt[fd].io_object, event);
  } else if (__fdt[fd].attr & FD_IS_PIPE) {
    return _is_blocking_pipe((pipe_end_t*)__fdt[fd].io_object, event);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    return _is_blocking_socket((socket_t*)__fdt[fd].io_object, event);
  } 
  assert(0 && "invalid fd");
}

static ssize_t _clean_read(int fd, void *buf, size_t count, off_t offset) {
  fd_entry_t *fde = &__fdt[fd];

  if (offset >= 0 && !(fde->attr & FD_IS_FILE)) {
    errno = EINVAL;
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    return _read_file((file_t*)fde->io_object, buf, count, offset);
  } else if (fde->attr & FD_IS_PIPE) {
    return _read_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _read_socket((socket_t*)fde->io_object, buf, count);
  } 
  assert(0 && "Invalid file descriptor");
}

static ssize_t _clean_write(int fd, const void *buf, size_t count, off_t offset) {

  fd_entry_t *fde = &__fdt[fd];

  if (offset >= 0 && !(fde->attr & FD_IS_FILE)) {
    errno = EINVAL;
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    return _write_file((file_t*)fde->io_object, buf, count, offset);
  } else if (fde->attr & FD_IS_PIPE) {
    return _write_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _write_socket((socket_t*)fde->io_object, buf, count);
  }
  assert(0 && "Invalid file descriptor");
}

////////////////////////////////////////////////////////////////////////////////

ssize_t _scatter_read(int fd, const struct iovec *iov, int iovcnt) {
  size_t count = 0;

  int i;
  for (i = 0; i < iovcnt; i++) {
    if (iov[i].iov_len == 0)
      continue;

    if (count > 0 && _is_blocking(fd, EVENT_READ))
      return count;

    ssize_t res = _clean_read(fd, iov[i].iov_base, iov[i].iov_len, -1);

    if (res == -1) {
      assert(count == 0);
      return res;
    }

    count += res;

    if ((size_t)res < iov[i].iov_len)
      break;
  }

  return count;
}

ssize_t _gather_write(int fd, const struct iovec *iov, int iovcnt) {
  size_t count = 0;

  int i;
  for (i = 0; i < iovcnt; i++) {
    if (iov[i].iov_len == 0)
      continue;

    // If we have something written, but now we blocked, we just return
    // what we have
    if (count > 0 && _is_blocking(fd, EVENT_WRITE))
      return count;

    ssize_t res = _clean_write(fd, iov[i].iov_base, iov[i].iov_len, -1);

    if (res == -1) {
      assert(count == 0);
      return res;
    }

    count += res;

    if ((size_t)res < iov[i].iov_len)
      break;
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
// FD specific POSIX routines
////////////////////////////////////////////////////////////////////////////////

static ssize_t _read(int fd, int type, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  //fprintf(stderr, "Reading %lu bytes of data from FD %d\n", count, fd);

  // Check for permissions
  if ((fde->io_object->flags & O_ACCMODE) == O_WRONLY) {
    klee_debug("Permission error (flags: %o)\n", fde->io_object->flags);
    errno = EBADF;
    return -1;
  }

  if (INJECT_FAULT(read, EINTR)) {
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    if (INJECT_FAULT(read, EIO)) {
      return -1;
    }
  } else if (fde->attr & FD_IS_SOCKET) {
    socket_t *sock = (socket_t*)fde->io_object;

    if (sock->status == SOCK_STATUS_CONNECTED &&
        INJECT_FAULT(read, ECONNRESET)) {
      return -1;
    }
  }

  // Now perform the real thing
  if (type == _IO_TYPE_SCATTER_GATHER) {
    va_list ap;
    va_start(ap, type);
    struct iovec *iov = va_arg(ap, struct iovec*);
    int iovcnt = va_arg(ap, int);
    va_end(ap);

    return _scatter_read(fd, iov, iovcnt);
  } else {
    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    off_t offset = -1;
    if (type == _IO_TYPE_POSITIONAL)
      offset = va_arg(ap, off_t);
    va_end(ap);

    return _clean_read(fd, buf, count, offset);
  }
}

DEFINE_MODEL(ssize_t, read, int fd, void *buf, size_t count) {
  return _read(fd, 0, buf, count);
}

DEFINE_MODEL(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt) {
  return _read(fd, _IO_TYPE_SCATTER_GATHER, iov, iovcnt);
}

DEFINE_MODEL(ssize_t, pread, int fd, void *buf, size_t count, off_t offset) {
  return _read(fd, _IO_TYPE_POSITIONAL, buf, count, offset);
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t _write(int fd, int type, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  // Check for permissions
  if ((fde->io_object->flags & O_ACCMODE) == O_RDONLY) {
    errno = EBADF;
    return -1;
  }

  if (INJECT_FAULT(write, EINTR)) {
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    if (INJECT_FAULT(write, EIO, EFBIG, ENOSPC)) {
      return -1;
    }
  } else if (fde->attr & FD_IS_SOCKET) {
    socket_t *sock = (socket_t*)fde->io_object;

    if (sock->status == SOCK_STATUS_CONNECTED &&
        INJECT_FAULT(write, ECONNRESET, ENOMEM)) {
      return -1;
    }
  }

  if (type == _IO_TYPE_SCATTER_GATHER) {
    va_list ap;
    va_start(ap, type);
    struct iovec *iov = va_arg(ap, struct iovec*);
    int iovcnt = va_arg(ap, int);
    va_end(ap);

    return _gather_write(fd, iov, iovcnt);
  } else {
    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    off_t offset = -1;
    if (type == _IO_TYPE_POSITIONAL)
      offset = va_arg(ap, off_t);
    va_end(ap);

    return _clean_write(fd, buf, count, offset);
  }
}

DEFINE_MODEL(ssize_t, write, int fd, const void *buf, size_t count) {
  return _write(fd, 0, buf, count);
}

DEFINE_MODEL(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt) {
  return _write(fd, _IO_TYPE_SCATTER_GATHER, iov, iovcnt);
}

DEFINE_MODEL(ssize_t, pwrite, int fd, const void *buf, size_t count, off_t offset) {
  return _write(fd, _IO_TYPE_POSITIONAL, buf, count, offset);
}

////////////////////////////////////////////////////////////////////////////////

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)out_fd)) {
    errno = EBADF;
    return -1;
  }

  if (!STATIC_LIST_CHECK(__fdt, (unsigned)in_fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *out_fde = &__fdt[out_fd];
  fd_entry_t *in_fde = &__fdt[in_fd];

  // Checking for permissions
  if ((out_fde->io_object->flags & O_ACCMODE) == O_RDONLY) {
    klee_debug("Permission error (flags: %o)\n", out_fde->io_object->flags);
    errno = EBADF;
    return -1;
  }

  if ((in_fde->io_object->flags & O_ACCMODE) == O_WRONLY) {
    klee_debug("Permission error (flags: %o)\n", in_fde->io_object->flags);
    errno = EBADF;
    return -1;
  }

  if (INJECT_FAULT(sendfile, ENOMEM, EIO)) {
    return -1;
  }

  if (offset != NULL) {
    if (lseek(in_fd, *offset, SEEK_SET) < 0)
      return -1;
  }

  off_t origpos = lseek(in_fd, 0, SEEK_CUR);

  // Now we do the actual transfer
  char buffer[SENDFILE_BUFFER_SIZE];

  size_t wtotal = 0; // Total bytes transferred
  size_t rtotal = 0; // Total bytes read

  size_t rpos = 0;
  size_t wpos = 0;
  ssize_t res = 0;

  while (count > 0) {
    if (_is_blocking(out_fd, EVENT_WRITE) && wtotal > 0) {
      break;
    }

    if (rpos - wpos == 0) {
      if (_is_blocking(in_fd, EVENT_READ) && wtotal > 0) {
        break;
      }

      res = _clean_read(in_fd, buffer,
          (count > SENDFILE_BUFFER_SIZE) ? SENDFILE_BUFFER_SIZE : count, -1);

      if (res <= 0) {
        // We don't assert anything here, since reads can be undone
        break;
      }

      rpos = res;
      wpos = 0;
      rtotal += res;
    }

    res = _clean_write(out_fd, &buffer[wpos], rpos - wpos, -1);
    if (res <= 0) {
      if (res == -1)
        assert(wtotal == 0);
      break;
    }

    wtotal += res;
    count -= res;
    wpos += res;
  }

  assert(wtotal <= rtotal);

  if (res < 0) {
    // Error, need to undo everything
    int _errno = errno;
    lseek(in_fd, origpos, SEEK_SET);

    errno = _errno;
    return -1;
  }

  if (offset)
    lseek(in_fd, origpos + wtotal, SEEK_SET);
  else
    lseek(in_fd, origpos, SEEK_SET);


  return wtotal;
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, close, int fd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  //klee_debug("Closing FD %d (pid %d)\n", fd, getpid());

  fd_entry_t *fde = &__fdt[fd];

  if (INJECT_FAULT(close, EINTR)) {
    return -1;
  }

  // Decrement the underlying IO object refcount
  assert(fde->io_object->refcount > 0);

  if (fde->io_object->refcount > 1) {
    fde->io_object->refcount--;
    // Just clear this FD
    STATIC_LIST_CLEAR(__fdt, fd);
    return 0;
  }

  int res;

  // Check the type of the descriptor
  if (fde->attr & FD_IS_FILE) {
    res = _close_file((file_t*)fde->io_object);
  } else if (fde->attr & FD_IS_PIPE) {
    res = _close_pipe((pipe_end_t*)fde->io_object);
  } else if (fde->attr & FD_IS_SOCKET) {
    res = _close_socket((socket_t*)fde->io_object);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }

  if (res == 0)
    STATIC_LIST_CLEAR(__fdt, fd);

  return res;
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, fstat, int fd, struct stat *buf) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_FILE) {
    return _stat_file((file_t*)fde->io_object, buf);
  } else if (fde->attr & FD_IS_PIPE) {
    return _stat_pipe((pipe_end_t*)fde->io_object, buf);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _stat_socket((socket_t*)fde->io_object, buf);
  } 
  assert(0 && "Invalid file descriptor");
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, dup3, int oldfd, int newfd, int flags) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (newfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  if (newfd == oldfd) {
    errno = EINVAL;
    return -1;
  }

  if (INJECT_FAULT(dup, EMFILE, EINTR)) {
    return -1;
  }

  if (STATIC_LIST_CHECK(__fdt, (unsigned)newfd)) {
    CALL_MODEL(close, newfd);
  }

  __fdt[newfd] = __fdt[oldfd];

  fd_entry_t *fde = &__fdt[newfd];
  fde->io_object->refcount++;

  if (flags & O_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  } else {
    fde->attr &= ~FD_CLOSE_ON_EXEC;
  }

  klee_debug("New duplicate of %d: %d\n", oldfd, newfd);
  return newfd;
}

DEFINE_MODEL(int, dup2, int oldfd, int newfd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (newfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  if (newfd == oldfd)
    return 0;

  return CALL_MODEL(dup3, oldfd, newfd, 0);
}

static int _dup(int oldfd, int startfd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (startfd < 0 || startfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  // Find the lowest unused file descriptor
  int fd;
  for (fd = startfd; fd < MAX_FDS; fd++) {
    if (!__fdt[fd].allocated)
      break;
  }

  if (fd == MAX_FDS) {
    errno = EMFILE;
    return -1;
  }

  return CALL_MODEL(dup2, oldfd, fd);
}

DEFINE_MODEL(int, dup, int oldfd) {
  return _dup(oldfd, 0);
}

// select() ////////////////////////////////////////////////////////////////////

static int _validate_fd_set(int nfds, fd_set *fds) {
  int res = 0;

  int fd;
  for (fd = 0; fd < nfds; fd++) {
    if (!_FD_ISSET(fd, fds))
      continue;

    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
      klee_warning("unallocated FD");
      return -1;
    }

    res++;
  }

  return res;
}

static int _register_events(int fd, wlist_id_t wlist, int events) {
  assert(STATIC_LIST_CHECK(__fdt, (unsigned)fd));

  if (__fdt[fd].attr & FD_IS_PIPE) {
    return _register_events_pipe((pipe_end_t*)__fdt[fd].io_object, wlist, events);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    return _register_events_socket((socket_t*)__fdt[fd].io_object, wlist, events);
  } 
  assert(0 && "invalid fd");
}

static void _deregister_events(int fd, wlist_id_t wlist, int events) {
  if(!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    // Silently exit
    return;
  }

  if (__fdt[fd].attr & FD_IS_PIPE) {
    _deregister_events_pipe((pipe_end_t*)__fdt[fd].io_object, wlist, events);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    _deregister_events_socket((socket_t*)__fdt[fd].io_object, wlist, events);
  } 
  assert(0 && "invalid fd");
}

// XXX Maybe we should break this into more pieces?
DEFINE_MODEL(int, select, int nfds, fd_set *readfds, fd_set *writefds,
    fd_set *exceptfds, struct timeval *timeout) {

  if (nfds < 0 || nfds > FD_SETSIZE) {
    errno = EINVAL;
    return -1;
  }

  int totalfds = 0;

  // Validating the fds
  if (readfds) {
    int res = _validate_fd_set(nfds, readfds);
    if (res ==  -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (writefds) {
    int res = _validate_fd_set(nfds, writefds);
    if (res == -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (exceptfds) {
    int res = _validate_fd_set(nfds, exceptfds);
    if (res == -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (INJECT_FAULT(select, ENOMEM)) {
    return -1;
  }

  if (timeout != NULL && totalfds == 0) {
    klee_warning("simulating timeout");
    // We just return timeout
    if (timeout->tv_sec != 0 || timeout->tv_usec != 0)
      _yield_sleep(timeout->tv_sec, timeout->tv_usec);

    return 0;
  }

  // Compute the minimum size of the FD set
  int setsize = ((nfds / NFDBITS) + ((nfds % NFDBITS) ? 1 : 0)) * (NFDBITS / 8);

  fd_set *out_readfds = NULL;
  fd_set *out_writefds = NULL;

  if (readfds) {
    out_readfds = (fd_set*)malloc(setsize);

    if(out_readfds == NULL) {
      errno = ENOMEM;
      return -1;
    }

    memset(out_readfds, 0, setsize);
  }

  if (writefds) {
    out_writefds = (fd_set*)malloc(setsize);

    if(out_writefds == NULL) {
      free(out_readfds);
      errno = ENOMEM;
      return -1;
    }

    memset(out_writefds, 0, setsize);
  }

  // No out_exceptfds here. This means that the thread will hang if select()
  // is called with FDs only in exceptfds.

  wlist_id_t wlist = 0;
  int res = 0;

  do {
    int fd;
    // First check to see if we have anything available
    for (fd = 0; fd < nfds; fd++) {
      if (readfds && _FD_ISSET(fd, readfds) && !_is_blocking(fd, EVENT_READ)) {
        _FD_SET(fd, out_readfds);
        res++;
      }
      if (writefds && _FD_ISSET(fd, writefds) && !_is_blocking(fd, EVENT_WRITE)) {
        _FD_SET(fd, out_writefds);
        res++;
      }
    }

    if (res > 0)
      break;

    // Nope, bad luck...

    // We wait until at least one FD becomes non-blocking

    // In particular, if all FD blocked, then all of them would be
    // valid FDs (none of them closed in the mean time)
    if (wlist == 0)
      wlist = klee_get_wlist();

    int fail = 0;

    // Register ourselves to the relevant FDs
    for (fd = 0; fd < nfds; fd++) {
      int events = 0;
      if (readfds && _FD_ISSET(fd, readfds)) {
        events |= EVENT_READ;
      }
      if (writefds && _FD_ISSET(fd, writefds)) {
        events |= EVENT_WRITE;
      }

      if (events != 0) {
        if (_register_events(fd, wlist, events) == -1) {
          fail = 1;
          break;
        }
      }
    }

    if (!fail)
      __thread_sleep(wlist);

    // Now deregister, in order to avoid useless notifications
    for (fd = 0; fd < nfds; fd++) {
      int events = 0;
      if (readfds && _FD_ISSET(fd, readfds)) {
        events |= EVENT_READ;
      }
      if (writefds && _FD_ISSET(fd, writefds)) {
        events |= EVENT_WRITE;
      }

      if (events != 0) {
        _deregister_events(fd, wlist, events);
      }
    }

    if (fail) {
      errno = ENOMEM;
      return -1;
    }
  } while (1);

  if (readfds) {
    memcpy(readfds, out_readfds, setsize);
    free(out_readfds);
  }

  if (writefds) {
    memcpy(writefds, out_writefds, setsize);
    free(out_writefds);
  }

  if (exceptfds) {
    memset(exceptfds, 0, setsize);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, fcntl, int fd, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  long arg;

  if (cmd == F_GETFD || cmd == F_GETFL || cmd == F_GETOWN || cmd == F_GETSIG
      || cmd == F_GETLEASE || cmd == F_NOTIFY) {
    arg = 0;
  } else {
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
  }

  fd_entry_t *fde = &__fdt[fd];
  int res = 0;

  switch (cmd) {
  case F_DUPFD:
  case F_DUPFD_CLOEXEC:
    res = _dup(fd, arg);
    if (res < -1) {
      return res;
    }

    if (cmd == F_DUPFD_CLOEXEC) {
      __fdt[res].attr |= FD_CLOEXEC;
    }
    break;

  case F_SETFD:
    if (arg & FD_CLOEXEC) {
      fde->attr |= FD_CLOSE_ON_EXEC;
    }
    break;
  case F_GETFD:
    res = (fde->attr & FD_CLOSE_ON_EXEC) ? FD_CLOEXEC : 0;
    break;

  case F_SETFL:
    if (arg & (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME)) {
      klee_warning("unsupported fcntl flags");
      errno = EINVAL;
      return -1;
    }
    fde->io_object->flags |= (arg & (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NONBLOCK));
    break;
  case F_GETFL:
    res = fde->io_object->flags;
    break;

  default:
    klee_warning("unsupported fcntl operation (EINVAL)");
    errno = EINVAL;
    return -1;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
// Forwarded / unsupported calls
////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, ioctl, int fd, unsigned long request, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  va_list ap;

  va_start(ap, request);
  char *argp = va_arg(ap, char*);

  va_end(ap);

  if (fde->attr & FD_IS_FILE) {
    return _ioctl_file((file_t*)fde->io_object, request, argp);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _ioctl_socket((socket_t*)fde->io_object, request, argp);
  } else {
    klee_warning("ioctl operation not supported on file descriptor");
    errno = EINVAL;
    return -1;
  }
}
