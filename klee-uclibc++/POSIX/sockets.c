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

#include "sockets.h"

#include "fd.h"

#include "signals.h"
#include "netlink.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <klee/klee.h>

#ifdef _FNC
#include "fnc_redhat.h"
#endif

#define CHECK_SEND_FLAGS(flags) \
		do { \
		  /*XXX: Ignore the MSG_NOSIGNAL flag since we don't support signals anyway*/ \
			if ((flags != 0) && (flags != MSG_NOSIGNAL)) { \
				klee_warning("sendto() flags unsupported for now"); \
				errno = EINVAL; \
				return -1;\
			}\
		} while (0)

#define CHECK_IS_SOCKET(fd) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) { \
    errno = EBADF; \
    return -1; \
    } \
    if (!(__fdt[fd].attr & FD_IS_SOCKET)) { \
      errno = ENOTSOCK; \
      return -1; \
    } \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
// Internal Routines
////////////////////////////////////////////////////////////////////////////////

static in_port_t __get_unused_port(void) {
  unsigned int i;
  char found = 0;

  while (!found) {
    found = 1;

    for (i = 0; i < MAX_PORTS; i++) {
      if (!STATIC_LIST_CHECK(__net.end_points, i))
        continue;

      struct sockaddr_in *addr = (struct sockaddr_in*) __net.end_points[i].addr;

      if (__net.next_port == addr->sin_port) {
        __net.next_port = htons(ntohs(__net.next_port) + 1);
        found = 0;
        break;
      }
    }
  }

  in_port_t res = __net.next_port;

  __net.next_port = htons(ntohs(__net.next_port) + 1);

  return res;
}

static pid_t __get_unused_nlpid(pid_t pid) {
  unsigned counter = 0;
  char found = 0;

  while (!found) {
    found = 1;
    pid_t candidate = pid | (counter << 16);

    unsigned i;
    for (i = 0; i < MAX_NETLINK_EPOINTS; i++) {
      if (!STATIC_LIST_CHECK(__netlink_net.end_points, i))
        continue;

      struct sockaddr_nl *addr = (struct sockaddr_nl*) __netlink_net.end_points[i].addr;

      if ((unsigned)candidate == addr->nl_pid) {
        counter++;
        found = 0;
        break;
      }
    }
  }

  return (pid | (counter << 16));
}

static end_point_t* __find_inet_end(const struct sockaddr_in* addr) {
  assert(addr != NULL);
  assert(addr->sin_port != 0);

  unsigned int i;
  for (i = 0; i < MAX_PORTS; i++) {
    if (!STATIC_LIST_CHECK(__net.end_points, i))
      continue;

    struct sockaddr_in *crt = (struct sockaddr_in*) __net.end_points[i].addr;

    if (crt->sin_port == addr->sin_port) {
      return &__net.end_points[i];
    }
  }
  return NULL;
}

static end_point_t *__get_inet_end(const struct sockaddr_in *addr) {
  if (addr->sin_addr.s_addr != INADDR_ANY && addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)
      && addr->sin_addr.s_addr != __net.net_addr.s_addr)
    return NULL;

  in_port_t port = addr->sin_port;
  if (port == 0) {
    port = __get_unused_port();
  } else {
    end_point_t* exising_endpoint = __find_inet_end(addr);
    if (exising_endpoint != NULL) {
      exising_endpoint->refcount++;
      return exising_endpoint;
    }
  }

  unsigned int i;
  STATIC_LIST_ALLOC(__net.end_points, i);

  if (i == MAX_PORTS)
    return NULL;

  __net.end_points[i].addr = (struct sockaddr*) malloc(sizeof(struct sockaddr_in));
  klee_make_shared(__net.end_points[i].addr, sizeof(struct sockaddr_in));
  __net.end_points[i].refcount = 1;
  __net.end_points[i].socket = NULL;

  struct sockaddr_in *newaddr = (struct sockaddr_in*) __net.end_points[i].addr;
  newaddr->sin_family = AF_INET;
  newaddr->sin_addr = __net.net_addr;
  newaddr->sin_port = port;

  return &__net.end_points[i];
}

static int __is_netlink_kernel(const struct sockaddr_nl *addr) {
  assert(addr != NULL);
  return addr->nl_pid == 0;
}

static end_point_t *__find_netlink_end(const struct sockaddr_nl *addr) {
  assert(addr != NULL);
  assert(addr->nl_pid != 0);

  unsigned int i;
  for (i = 0; i < MAX_NETLINK_EPOINTS; i++) {
    if (!STATIC_LIST_CHECK(__netlink_net.end_points, i))
      continue;

    struct sockaddr_nl *crt = (struct sockaddr_nl*) __netlink_net.end_points[i].addr;

    if (crt->nl_pid == addr->nl_pid) {
      return &__netlink_net.end_points[i];
    }
  }

  return NULL;
}

static end_point_t *__get_netlink_end(const struct sockaddr_nl *addr) {
  pid_t pid = addr->nl_pid;
  if (pid == 0) {
    pid = __get_unused_nlpid(getpid());
  } else {
    end_point_t *existing_endpoint = __find_netlink_end(addr);
    if (existing_endpoint != NULL) {
      existing_endpoint->refcount++;
      return existing_endpoint;
    }
  }

  unsigned int i;
  STATIC_LIST_ALLOC(__netlink_net.end_points, i);

  if (i == MAX_NETLINK_EPOINTS)
    return NULL;

  __netlink_net.end_points[i].addr = (struct sockaddr*) malloc(sizeof(struct sockaddr_nl));
  klee_make_shared(__netlink_net.end_points[i].addr, sizeof(struct sockaddr_nl));
  __netlink_net.end_points[i].refcount = 1;
  __netlink_net.end_points[i].socket = NULL;

  struct sockaddr_nl *newaddr = (struct sockaddr_nl*) __netlink_net.end_points[i].addr;
  newaddr->nl_family = AF_NETLINK;
  newaddr->nl_groups = 0;
  newaddr->nl_pad = 0;
  newaddr->nl_pid = pid;

  return &__netlink_net.end_points[i];
}

static end_point_t *__get_unix_end(const struct sockaddr_un *addr) {
  unsigned int i;
  if (addr) {
    // Search for an existing one...
    for (i = 0; i < MAX_UNIX_EPOINTS; i++) {
      if (!STATIC_LIST_CHECK(__unix_net.end_points, i))
        continue;

      struct sockaddr_un *crt = (struct sockaddr_un*) __unix_net.end_points[i].addr;

      if (!crt)
        continue;

      if (strcmp(addr->sun_path, crt->sun_path) == 0) {
        __unix_net.end_points[i].refcount++;
        return &__unix_net.end_points[i];
      }
    }
  }

  // Create a new end point
  STATIC_LIST_ALLOC(__unix_net.end_points, i);

  if (i == MAX_UNIX_EPOINTS)
    return NULL;

  __unix_net.end_points[i].addr = NULL;
  __unix_net.end_points[i].refcount = 1;
  __unix_net.end_points[i].socket = NULL;

  if (addr) {
    size_t addrsize = sizeof(addr->sun_family) + strlen(addr->sun_path) + 1;
    __unix_net.end_points[i].addr = (struct sockaddr*) malloc(addrsize);
    klee_make_shared(__unix_net.end_points[i].addr, addrsize);

    struct sockaddr_un *newaddr = (struct sockaddr_un*) __unix_net.end_points[i].addr;

    newaddr->sun_family = AF_UNIX;
    strcpy(newaddr->sun_path, addr->sun_path);
  }

  return &__unix_net.end_points[i];
}
static void __release_end_point(end_point_t *end_point) {
  assert(end_point->allocated);
  assert(end_point->refcount > 0);

  end_point->refcount--;
  if (end_point->refcount == 0) {
    if (end_point->addr)
      free(end_point->addr);

    memset(end_point, 0, sizeof(end_point_t));
  }
}

static int __create_shared_datagram(datagram_t* datagram,
    const struct sockaddr* addr, size_t addr_len,
    const struct iovec *iov, int iovcnt) {

  size_t count = __count_iovec(iov, iovcnt);

  // Capping to the max dgram size
  count = (count > MAX_DGRAM_SIZE) ? MAX_DGRAM_SIZE : count;

  _block_init(&datagram->contents, count);
  klee_make_shared(datagram->contents.contents, count);

  // Copying address info
  datagram->src = (struct sockaddr*)malloc(addr_len);
  datagram->src_len = addr_len;

  klee_make_shared(datagram->src, addr_len);
  memcpy(datagram->src, addr, addr_len);

  ssize_t res = _block_writev(&datagram->contents, iov, iovcnt, count, 0);
  assert((size_t)res == count);

  return 0;
}

static void __free_datagram(datagram_t* datagram) {
  _block_finalize(&datagram->contents);

  free(datagram->src);
}

static void __free_all_datagrams(stream_buffer_t* buf) {
  while (!_stream_is_empty(buf)) {
    datagram_t datagram;
    ssize_t read = _stream_read(buf, (char*)&datagram, sizeof(datagram_t)) ;
    assert(read == sizeof(datagram_t));
    __free_datagram(&datagram);
  }
}

// close() /////////////////////////////////////////////////////////////////////

static int _bind(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen);

static int __is_bound(socket_t* sock) {
  assert(sock->type == SOCK_DGRAM || sock->type == SOCK_RAW);

  return sock->local_end != NULL;
}

static int __autobind(socket_t* sock) {
  assert(sock->status == SOCK_STATUS_CREATED || sock->status == SOCK_STATUS_CONNECTED);
  assert(sock->local_end == NULL);

  if (sock->domain == AF_INET) {
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = 0,
        .sin_addr = __net.net_addr };

    return _bind(sock, (struct sockaddr*) &addr, sizeof(addr));

  } else if (sock->domain == AF_UNIX) {
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    return _bind(sock, (struct sockaddr*) &addr, sizeof(addr.sun_family));

  } else if (sock->domain == AF_NETLINK) {
    struct sockaddr_nl addr = { .nl_family = AF_NETLINK, .nl_pad = 0,
        .nl_pid = 0, .nl_groups = 0 };
    return _bind(sock, (struct sockaddr*) &addr, sizeof(addr));

  } else {
    assert(0 && "invalid socket domain");
  }
}

static void _shutdown(socket_t *sock, int how);

int _close_socket(socket_t *sock) {
  // First, do state specific clean-up

  if (sock->status == SOCK_STATUS_CONNECTED) {
    _shutdown(sock, SHUT_RDWR);
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    while (!_stream_is_empty(sock->listen)) {
      socket_t *remote;
      ssize_t res = _stream_read(sock->listen, (char*) &remote, sizeof(socket_t*));
      assert(res == sizeof(socket_t*));

      remote->__bdata.queued--;

      if (remote->status == SOCK_STATUS_CLOSING) {
        if (remote->__bdata.queued == 0)
          free(remote);

      } else {
        assert(remote->conn_wlist > 0);
        __thread_notify_all(remote->conn_wlist);

        _event_queue_notify(remote->conn_evt_queue);
        _event_queue_finalize(remote->conn_evt_queue);
        free(remote->conn_evt_queue);

        remote->conn_evt_queue = NULL;
        remote->conn_wlist = 0;
        remote->status = SOCK_STATUS_CREATED;

        __release_end_point(remote->remote_end);
        sock->remote_end = NULL;
      }
    }

    _stream_destroy(sock->listen);
  }

  if (sock->status == SOCK_STATUS_CONNECTING) {
    assert(sock->conn_wlist > 0);

    __thread_notify_all(sock->conn_wlist);
    _event_queue_notify(sock->conn_evt_queue);

    _event_queue_finalize(sock->conn_evt_queue);
    free(sock->conn_evt_queue);
  }

  // Now clean-up the endpoints

  if (sock->local_end) {
    sock->local_end->socket = NULL;
    __release_end_point(sock->local_end);
    sock->local_end = NULL;
  }

  if (sock->remote_end) {
    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
  }

  // Mark us as closing, so the returning operations know to release us
  sock->status = SOCK_STATUS_CLOSING;

  if (sock->__bdata.queued == 0) {
    free(sock);
  }

  return 0;
}

// read() //////////////////////////////////////////////////////////////////////

static ssize_t __read_stream_socket_raw(socket_t* sock, void *buf, size_t count) {
  if (sock->__bdata.flags & O_NONBLOCK) {
    if (_stream_is_empty(sock->in) && !_stream_is_closed(sock->in)) {
      errno = EAGAIN;
      return -1;
    }
  }

  sock->__bdata.queued++;
  ssize_t res = _stream_read(sock->in, buf, count);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSING) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  if (res == -1) {
    // The stream was shut down in the mean time
    errno = EINVAL;
  }

  return res;
}

static ssize_t __read_datagram_socket(socket_t *sock, const struct iovec *iov,
    int iovcnt, struct sockaddr* addr, socklen_t* addr_len) {

  if (!__is_bound(sock))
    klee_stack_trace();

  assert(__is_bound(sock) && "read: dgram socket autobind is not supported");

  datagram_t datagram;
  ssize_t dgram_read = __read_stream_socket_raw(sock, (void*) &datagram, sizeof(datagram_t));
  if(dgram_read == -1)
    return -1;

  assert(dgram_read == sizeof(datagram_t));

  // Compute the total size of the buffers
  size_t count = __count_iovec(iov, iovcnt);
  if (count == 0)
    return 0;

  count = count > datagram.contents.size ? datagram.contents.size : count;

  ssize_t res = _block_readv(&datagram.contents, iov, iovcnt, count, 0);
  assert((size_t)res == count);

  if (addr != NULL) {
    memcpy(addr, datagram.src, (*addr_len > datagram.src_len) ? datagram.src_len : *addr_len);
    *addr_len = datagram.src_len;
  }

  __free_datagram(&datagram);

  return count;
}

static ssize_t __read_stream_socket(socket_t* sock, void* buf, size_t count) {
  if(sock->status != SOCK_STATUS_CONNECTED) {
    errno = ENOTCONN;
    return -1;
  }

  if (sock->in == NULL) // The socket is shut down for reading
    return 0;

  if (count == 0)
    return 0;

  return __read_stream_socket_raw(sock, buf, count);
}

ssize_t _read_socket(socket_t *sock, void *buf, size_t count) {
  if (sock->type == SOCK_STREAM) {
    return __read_stream_socket(sock, buf, count);
  } else if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    struct iovec iov = { .iov_base = buf, .iov_len = count };
    return __read_datagram_socket(sock, &iov, 1, NULL, NULL);
  } else {
    assert(0 && "invalid socket type");
  }
}

// write() /////////////////////////////////////////////////////////////////////

static ssize_t __write_datagram_to_stream(stream_buffer_t *stream,
    const struct sockaddr* from, size_t addr_len,
    const struct iovec *iov, int iovcnt) {

  if (stream->max_size - stream->size < sizeof(datagram_t)) {
    errno = ENOBUFS;
    return -1;
  }

  datagram_t datagram;
  if(__create_shared_datagram(&datagram, from, addr_len, iov, iovcnt) == -1)
    return -1;

  size_t written = _stream_write(stream, (char*) &datagram, sizeof(datagram_t));
  assert(written == sizeof(datagram_t));

  return datagram.contents.size;
}

//non blocking call
static ssize_t __write_datagram_socket(socket_t *sock,
    const struct iovec *iov, int iovcnt,
    const struct sockaddr* addr, size_t addr_len) {

  assert(sock->status != SOCK_STATUS_CLOSING && "impossible");

  if(addr == NULL && sock->status != SOCK_STATUS_CONNECTED) {
    errno = EDESTADDRREQ;
    return -1;
  }

  ssize_t res = -1;
  char done = 0;

  if (!__is_bound(sock)) {
    if(__autobind(sock) == -1)
      return -1;
  }

  if (sock->domain == AF_NETLINK) {
    if (__is_netlink_kernel((const struct sockaddr_nl*)addr)) {
      res = __count_iovec(iov, iovcnt);
      _netlink_handler(sock, iov, iovcnt, res);
      done = 1;
    }
  }

  if (!done) {
    const end_point_t* remote_end;
    if (addr != NULL) {
      if (sock->domain == AF_NETLINK)
        remote_end = __find_netlink_end((const struct sockaddr_nl*)addr);
      else
        remote_end = __find_inet_end((const struct sockaddr_in*)addr);
    } else {
      remote_end = sock->remote_end;
      assert(sock->remote_end != NULL);
    }

    if (remote_end == NULL || remote_end->socket == NULL || remote_end->socket->in == NULL)
      res = __count_iovec(iov, iovcnt); //no such remote in net, closed, shutdown
    else
      res = __write_datagram_to_stream(remote_end->socket->in,
          sock->local_end->addr, sizeof(struct sockaddr_in), iov, iovcnt);

  }

  return res;
}

static ssize_t __write_stream_socket(socket_t *sock, const char *buf, size_t count) {
  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = ENOTCONN;
    return -1;
  }

  stream_buffer_t* out_stream = sock->out;

  if (out_stream == NULL || _stream_is_closed(out_stream)) {
    // The socket is shut down for writing
    errno = EPIPE;
    return -1;
  }

  if (count == 0) {
    return 0;
  }

  if (sock->__bdata.flags & O_NONBLOCK) {
    if (_stream_is_full(out_stream) & !_stream_is_closed(out_stream)) {
      errno = EAGAIN;
      return -1;
    }
  }

  sock->__bdata.queued++;
  ssize_t res = _stream_write(out_stream, buf, count);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSING) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  if (res == -1) {
    // The stream was shut down in the mean time
    errno = EINVAL;
  }

  return res;
}

ssize_t _write_socket(socket_t *sock, const void *buf, size_t count) {
  if (sock->type == SOCK_STREAM) {
    return __write_stream_socket(sock, buf, count);
  } else if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    struct iovec iov = { .iov_base = (void*)buf, .iov_len = count };
    return __write_datagram_socket(sock, &iov, 1, NULL, 0);
  } else {
    assert(0 && "invalid socket type");
  }
}

// fstat() /////////////////////////////////////////////////////////////////////

int _stat_socket(socket_t *sock, struct stat *buf) {
  assert(0 && "not implemented");
}

// ioctl() /////////////////////////////////////////////////////////////////////

int _ioctl_socket(socket_t *sock, unsigned long request, char *argp) {
  switch (request) {
  case KLEE_SIO_SYMREADS:
    if (sock->status != SOCK_STATUS_CONNECTED || sock->out == NULL) {
      errno = ENOTTY;
      return -1;
    }

    assert(sock->type != SOCK_DGRAM && sock->type != SOCK_RAW);

    sock->out->status |= BUFFER_STATUS_SYM_READS;
    return 0;
  case KLEE_SIO_READCAP:
    if (sock->status != SOCK_STATUS_CONNECTED || sock->out == NULL) {
      errno = ENOTTY;
      return -1;
    }

    _stream_set_rsize(sock->out, (size_t) argp);
    return 0;
  case FIONREAD:
    if (sock->status != SOCK_STATUS_CONNECTED || sock->in == NULL) {
      *((int*) argp) = 0;
      return 0;
    }
    *((int*) argp) = sock->in->size;
    return 0;
  default:
    errno = EINVAL;
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////

int _is_blocking_socket(socket_t *sock, int event) {
  assert((event == EVENT_READ) || (event == EVENT_WRITE));

  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    assert(__is_bound(sock) && "only bound sockets are supported");

    if (event == EVENT_READ)
      return _stream_is_empty(sock->in) && !_stream_is_closed(sock->in);
    else
      return 0;
  }

  if (sock->status == SOCK_STATUS_CREATED)
    return 0;//TODO: ah: strange: why not 1??

  if (sock->status == SOCK_STATUS_CONNECTING)
    return event == EVENT_WRITE;

  if (sock->status == SOCK_STATUS_CONNECTED) {
    switch (event) {
    case EVENT_READ:
      return sock->in && _stream_is_empty(sock->in) && !_stream_is_closed(sock->in);
    case EVENT_WRITE:
      return sock->out && _stream_is_full(sock->out) && !_stream_is_closed(sock->out);
    }
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    switch (event) {
    case EVENT_READ:
      assert (!_stream_is_closed(sock->listen) && "invalid socket state");
      return _stream_is_empty(sock->listen);
    case EVENT_WRITE:
      return 0;
    }
  }

  // We should never reach here...
  assert(0 && "invalid socket state");
}

int _register_events_socket(socket_t *sock, wlist_id_t wlist, int events) {

  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    assert(__is_bound(sock) && "only bound sockets are supported");
    assert(sock->status == SOCK_STATUS_CONNECTED || sock->status == SOCK_STATUS_CREATED);

    if (events & EVENT_READ) {
      return _stream_register_event(sock->in, wlist);
    }
    //ignore EVENT_WRITE
    return 0;
  }

  if (sock->status == SOCK_STATUS_CONNECTED) {
    if ((events & EVENT_READ) && _stream_register_event(sock->in, wlist) == -1)
      return -1;

    if ((events & EVENT_WRITE) && _stream_register_event(sock->out, wlist) == -1) {
      _stream_clear_event(sock->in, wlist);
      return -1;
    }
    return 0;
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    return _stream_register_event(sock->listen, wlist);
  }

  if (sock->status == SOCK_STATUS_CONNECTING) {
    return _event_queue_register(sock->conn_evt_queue, wlist);
  }

  // We should never reach here
  assert(0 && "invalid socket state");
}

void _deregister_events_socket(socket_t *sock, wlist_id_t wlist, int events) {

  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    assert(__is_bound(sock) && "only bound sockets are supported");
    assert(sock->status == SOCK_STATUS_CONNECTED || sock->status == SOCK_STATUS_CREATED);

    if (events & EVENT_READ)
      _stream_clear_event(sock->in, wlist);

    //ignore EVENT_WRITE
    return;
  }

  if (sock->status == SOCK_STATUS_CONNECTED) {
    if ((events & EVENT_READ) && sock->in)
      _stream_clear_event(sock->in, wlist);

    if ((events & EVENT_WRITE) && sock->out)
      _stream_clear_event(sock->out, wlist);
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    _stream_clear_event(sock->listen, wlist);
  }
}

////////////////////////////////////////////////////////////////////////////////
// The Sockets API
////////////////////////////////////////////////////////////////////////////////

static int _validate_net_socket(int domain, int type, int protocol) {
  assert(domain == AF_INET || domain == AF_UNIX);

  switch (type) {
  case SOCK_STREAM:
    if (protocol != 0 && protocol != IPPROTO_TCP) {
      klee_warning("unsupported protocol");
      errno = EPROTONOSUPPORT;
      return -1;
    }
    break;
  case SOCK_DGRAM:
    if (domain == AF_UNIX) {
      klee_warning("unsupported protocol: UDP for AF_INET only"); //TODO: ah: implement
      errno = EPROTONOSUPPORT;
      return -1;
    }
    if (protocol != 0 && protocol != IPPROTO_UDP) {
      klee_warning("unsupported protocol");
      errno = EPROTONOSUPPORT;
      return -1;
    }
    break;
  default:
    klee_warning("unsupported socket type");
    errno = EPROTONOSUPPORT;
    return -1;
  }

  return 0;
}

static int _validate_netlink_socket(int domain, int type, int protocol) {
  assert(domain == AF_NETLINK);

  if (type != SOCK_RAW && type != SOCK_DGRAM) {
    klee_warning("unsupported netlink socket type");
    errno = EPROTONOSUPPORT;
    return -1;
  }

  switch (protocol) {
  case NETLINK_ROUTE:
    // Supported
    return 0;
  default:
    klee_warning("unsupported netlink family");
    errno = EPROTONOSUPPORT;
    return -1;
  }
}

int socket(int domain, int type, int protocol) {
  klee_debug("Attempting to create a socket: domain = %d, type = %d, protocol = %d\n", domain,
      type, protocol);
  int base_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
  int res;
  // Check the validity of the request
  switch (domain) {
  case AF_INET:
  case AF_UNIX:
    res = _validate_net_socket(domain, base_type, protocol);
    if (res == -1)
      return -1;
    break;
  case AF_NETLINK:
    res = _validate_netlink_socket(domain, base_type, protocol);
    if (res == -1)
      return -1;
    break;
  default:
    klee_warning("unsupported socket domain");
    errno = EINVAL;
    return -1;
  }

  if (INJECT_FAULT(socket, EACCES, EMFILE, ENFILE, ENOBUFS, ENOMEM)) {
    return -1;
  }

  // Now let's obtain a new file descriptor
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);

  if (fd == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];
  fde->attr |= FD_IS_SOCKET;

  if (type & SOCK_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  }

  // Create the socket object
  socket_t *sock = (socket_t*) malloc(sizeof(socket_t));
  klee_make_shared(sock, sizeof(socket_t));
  memset(sock, 0, sizeof(socket_t));

  sock->__bdata.flags |= O_RDWR;
  if (type & SOCK_NONBLOCK) {
    sock->__bdata.flags |= O_NONBLOCK;
  }

  sock->__bdata.refcount = 1;
  sock->__bdata.queued = 0;

  sock->status = SOCK_STATUS_CREATED;
  sock->type = base_type;
  sock->domain = domain;
  sock->protocol = protocol;

  fde->io_object = (file_base_t*) sock;

  klee_debug("Socket created: fd = %d\n", fd);
  return fd;
}

// bind() //////////////////////////////////////////////////////////////////////

static int _bind(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->local_end != NULL) {
    errno = EINVAL;
    return -1;
  }

  if (addr->sa_family != sock->domain) {
    errno = EINVAL;
    return -1;
  }

  if (INJECT_FAULT(bind, EADDRINUSE)) {
    return -1;
  }

  end_point_t *end_point;

  if (sock->domain == AF_INET) {
    end_point = __get_inet_end((struct sockaddr_in*) addr);
  } else if (sock->domain == AF_UNIX) {
    end_point = __get_unix_end((struct sockaddr_un*) addr);
  } else if (sock->domain == AF_NETLINK) {
    end_point = __get_netlink_end((struct sockaddr_nl*) addr);
  } else {
    assert(0 && "invalid socket");
  }

  if (end_point == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (end_point->socket != NULL) {
    __release_end_point(end_point);
    errno = EADDRINUSE;
    return -1;
  }

  sock->local_end = end_point;
  end_point->socket = sock;

  if(sock->type == SOCK_DGRAM || sock->type == SOCK_RAW)
    sock->in = _stream_create(sizeof(datagram_t) * MAX_NUMBER_DGRAMS, 1); //todo: refactor when implement shutdown for write part

  return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  return _bind(sock, addr, addrlen);
}

// get{sock,peer}name() ////////////////////////////////////////////////////////

static void _get_endpoint_name(socket_t *sock, end_point_t *end_point, struct sockaddr *addr,
    socklen_t *addrlen) {
  if (sock->domain == AF_INET) {
    socklen_t len = *addrlen;

    if (len > sizeof(struct sockaddr_in))
      len = sizeof(struct sockaddr_in);

    memcpy(addr, end_point->addr, len);
    *addrlen = sizeof(struct sockaddr_in);
  } else if (sock->domain == AF_UNIX) {
    socklen_t len = *addrlen;

    if (end_point->addr) {
      struct sockaddr_un *ep_addr = (struct sockaddr_un*) end_point->addr;
      socklen_t ep_len = sizeof(ep_addr->sun_family) + strlen(ep_addr->sun_path) + 1;

      if (len > ep_len)
        len = ep_len;

      memcpy(addr, ep_addr, len);
      *addrlen = ep_len;
    } else {
      struct sockaddr_un tmp;
      tmp.sun_family = AF_UNIX;

      if (len > sizeof(tmp.sun_family))
        len = sizeof(tmp.sun_family);

      memcpy(addr, &tmp, sizeof(tmp.sun_family));
      *addrlen = sizeof(tmp.sun_family);
    }
  } else {
    assert(0 && "invalid socket");
  }
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (sock->local_end == NULL) {
    errno = EINVAL;
    return -1;
  }

  _get_endpoint_name(sock, sock->local_end, addr, addrlen);

  return 0;
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (sock->remote_end == NULL) {
    errno = ENOTCONN;
    return -1;
  }

  _get_endpoint_name(sock, sock->remote_end, addr, addrlen);

  return 0;
}

// listen() ////////////////////////////////////////////////////////////////////

int listen(int sockfd, int backlog) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (sock->type != SOCK_STREAM) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (sock->local_end == NULL || (sock->status != SOCK_STATUS_CREATED && sock->status
      != SOCK_STATUS_LISTENING)) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (INJECT_FAULT(listen, EADDRINUSE)) {
    return -1;
  }

  // Create the listening queue
  if (sock->status != SOCK_STATUS_LISTENING) {
    sock->status = SOCK_STATUS_LISTENING;
    sock->listen = _stream_create(backlog * sizeof(socket_t*), 1);
  }

  return 0;
}

// connect() ///////////////////////////////////////////////////////////////////

static int _inet_stream_reach(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->local_end == NULL) {
    // We obtain a local bind point
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr = __net.net_addr;
    local_addr.sin_port = 0; // Get an unused port

    sock->local_end = __get_inet_end(&local_addr);

    if (sock->local_end == NULL) {
      errno = EAGAIN;
      return -1;
    }
  }

  assert(sock->remote_end == NULL);

  if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  sock->remote_end = __get_inet_end((struct sockaddr_in*) addr);
  if (!sock->remote_end) {
    errno = EINVAL;
    return -1;
  }

  return 0;
}

static int _unix_stream_reach(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->local_end == NULL) {
    // We obtain an anonymous UNIX bind point
    sock->local_end = __get_unix_end(NULL);

    if (sock->local_end == NULL) {
      errno = EAGAIN;
      return -1;
    }
  }

  assert(sock->remote_end == NULL);

  if (addr->sa_family != AF_UNIX || addrlen < sizeof(addr->sa_family) + 1) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  sock->remote_end = __get_unix_end((struct sockaddr_un*) addr);

  if (!sock->remote_end) {
    errno = EINVAL;
    return -1;
  }

  return 0;
}

static int _datagram_connect(int sockfd, socket_t *sock, const struct sockaddr *addr,
    socklen_t addrlen) {
  assert(sock->domain == AF_INET && "only AF_INET is supported");

  if (sock->status != SOCK_STATUS_CREATED && sock->status != SOCK_STATUS_CONNECTED) {
    errno = EINVAL;
    return -1;
  }

  //reset previous connection
  if (sock->status == SOCK_STATUS_CONNECTED) {
    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
  }

  if (sock->remote_end == NULL) {
    sock->remote_end = __get_inet_end((struct sockaddr_in*) addr);
    if (!sock->remote_end) {
      errno = EINVAL;
      return -1;
    }
  }

  sock->status = SOCK_STATUS_CONNECTED;
  return 0;
}

static int _stream_connect(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->status != SOCK_STATUS_CREATED) {
    if (sock->status == SOCK_STATUS_CONNECTED) {
      errno = EISCONN;
      return -1;
    } else {
      errno = EINVAL;
      return -1;
    }
  }

  // Let's obtain the end points
  if (sock->domain == AF_INET) {
    if (_inet_stream_reach(sock, addr, addrlen) == -1)
      return -1;
  } else if (sock->domain == AF_UNIX) {
    if (_unix_stream_reach(sock, addr, addrlen) == -1)
      return -1;
  } else {
    assert(0 && "invalid socket");
  }

  if (sock->remote_end->socket == NULL || sock->remote_end->socket->status != SOCK_STATUS_LISTENING
      || sock->remote_end->socket->domain != sock->domain) {

    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
    errno = ECONNREFUSED;
    return -1;
  }

  socket_t *remote = sock->remote_end->socket;

  if (_stream_is_full(remote->listen)) {
    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
    errno = ECONNREFUSED;
    return -1;
  }

  sock->status = SOCK_STATUS_CONNECTING;

  // We queue ourselves in the list...
  ssize_t res = _stream_write(remote->listen, (char*) &sock, sizeof(socket_t*));
  assert(res == sizeof(socket_t*));
  sock->__bdata.queued++; // Now we're also referenced by the dest. socket

  sock->conn_wlist = klee_get_wlist();
  sock->conn_evt_queue = (event_queue_t*) malloc(sizeof(event_queue_t));
  klee_make_shared(sock->conn_evt_queue, sizeof(event_queue_t));
  _event_queue_init(sock->conn_evt_queue, MAX_EVENTS, 1);

  if (sock->__bdata.flags & O_NONBLOCK) {
    errno = EINPROGRESS;
    return -1;
  }

  // ... and we wait for a notification
  sock->__bdata.queued++;
  __thread_sleep(sock->conn_wlist);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSING) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = ECONNREFUSED;
    return -1;
  }

  return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CHECK_IS_SOCKET(sockfd);
  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (INJECT_FAULT(connect, EACCES, EPERM, ECONNREFUSED)) {
    return -1;
  }

  if (sock->type == SOCK_STREAM) {
    return _stream_connect(sock, addr, addrlen);
  } else if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    return _datagram_connect(sockfd, sock, addr, addrlen);
  } else {
    assert(0 && "invalid socket");
  }
}

// accept() ////////////////////////////////////////////////////////////////////

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (sock->type != SOCK_STREAM) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (sock->status != SOCK_STATUS_LISTENING) {
    errno = EINVAL;
    return -1;
  }

  if (INJECT_FAULT(accept, ENOMEM, ENOBUFS, EPROTO, EPERM)) {
    return -1;
  }

  if (sock->__bdata.flags & O_NONBLOCK) {
    if (_stream_is_empty(sock->listen)) {
      errno = EAGAIN;
      return -1;
    }
  }

  socket_t *remote, *local;
  end_point_t *end_point;

  sock->__bdata.queued++;
  ssize_t res = _stream_read(sock->listen, (char*) &remote, sizeof(socket_t*));
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSING) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  assert(res == sizeof(socket_t*));

  remote->__bdata.queued--;

  if (remote->status == SOCK_STATUS_CLOSING) {
    if (remote->__bdata.queued == 0) {
      free(remote);
    }

    errno = ECONNABORTED;
    return -1;
  }

  assert(remote->status == SOCK_STATUS_CONNECTING);
  assert(remote->conn_wlist > 0);

  __thread_notify_all(remote->conn_wlist);
  _event_queue_notify(remote->conn_evt_queue);
  remote->conn_wlist = 0;
  _event_queue_finalize(remote->conn_evt_queue);
  free(remote->conn_evt_queue);
  remote->conn_evt_queue = NULL;

  int failure = 0;
  // Get a new local port for the connection
  if (sock->domain == AF_INET) {
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr = __net.net_addr;
    local_addr.sin_port = 0; // Find an unused port

    end_point = __get_inet_end(&local_addr);

    if (end_point == NULL) {
      errno = EINVAL;
      failure = 1;
    }
  } else if (sock->domain == AF_UNIX) {
    end_point = __get_unix_end(NULL);

    if (end_point == NULL) {
      errno = EINVAL;
      failure = 1;
    }
  } else {
    assert(0 && "invalid socket");
  }

  // Get a new file descriptor for the new socket
  int fd;
  if (!failure) {
    STATIC_LIST_ALLOC(__fdt, fd);
    if (fd == MAX_FDS) {
      __release_end_point(end_point);
      errno = ENFILE;
      failure = 1;
    }
  }

  if (failure) {
    remote->status = SOCK_STATUS_CREATED;

    __release_end_point(remote->remote_end);
    sock->remote_end = NULL;

    return -1;
  }

  // Setup socket attributes
  local = (socket_t*) malloc(sizeof(socket_t));
  klee_make_shared(local, sizeof(socket_t));
  memset(local, 0, sizeof(socket_t));

  __fdt[fd].attr |= FD_IS_SOCKET;
  __fdt[fd].io_object = (file_base_t*) local;

  local->__bdata.flags |= O_RDWR;
  local->__bdata.refcount = 1;
  local->__bdata.queued = 0;

  local->status = SOCK_STATUS_CONNECTED;
  local->domain = remote->domain;
  local->type = SOCK_STREAM;

  // Setup end points
  local->local_end = end_point;
  end_point->socket = local;
  __release_end_point(remote->remote_end);
  remote->remote_end = end_point;
  remote->remote_end->refcount++;

  local->remote_end = remote->local_end;
  local->remote_end->refcount++;

  // Setup streams
  local->in = _stream_create(STREAM_BUFFER_SIZE, 1);
  remote->out = local->in;

  local->out = _stream_create(STREAM_BUFFER_SIZE, 1);
  remote->in = local->out;

  // All is good for the remote socket
  remote->status = SOCK_STATUS_CONNECTED;
  if (addr != NULL)
    _get_endpoint_name(local, local->remote_end, addr, addrlen);

  klee_debug("Connected socket created: %d\n", fd);

  // Now return in our process
  return fd;
}

int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
  int connsockfd = accept(sockfd, addr, addrlen);

  if (connsockfd == -1)
    return -1;

  fd_entry_t *fde = &__fdt[connsockfd];

  if (flags & SOCK_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  }

  if (flags & SOCK_NONBLOCK) {
    fde->io_object->flags |= O_NONBLOCK;
  }

  return connsockfd;
}

// shutdown() //////////////////////////////////////////////////////////////////

static void _shutdown(socket_t *sock, int how) {
  if ((how == SHUT_RD || how == SHUT_RDWR) && sock->in != NULL) {
    if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) { //todo: ah: add support for shutting down SHUT_WR part of UDP socket
      __free_all_datagrams(sock->in);
      _stream_destroy(sock->in);
      sock->in = NULL;
      return;
    }

    // Shutting down the reading part...
    if (_stream_is_closed(sock->in)) {
      _stream_destroy(sock->in);
    } else {
      _stream_close(sock->in);
    }

    sock->in = NULL;
  }

  if ((how == SHUT_WR || how == SHUT_RDWR) && sock->out != NULL) {
    // Shutting down the writing part...
    if (_stream_is_closed(sock->out)) {
      _stream_destroy(sock->out);
    } else {
      _stream_close(sock->out);
    }

    sock->out = NULL;
  }
}

int shutdown(int sockfd, int how) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = ENOTCONN;
    return -1;
  }

  _shutdown(sock, how);

  return 0;
}

// send()/recv() ///////////////////////////////////////////////////////////////

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
  CHECK_IS_SOCKET(sockfd);
  CHECK_SEND_FLAGS(flags);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;
  return _write_socket(sock, buf, len);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  CHECK_IS_SOCKET(sockfd);
  if (flags != 0) {
    klee_warning("recv() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  return _read_socket(sock, buf, len);
}

// sendto()/recvfrom() /////////////////////////////////////////////////////////
ssize_t sendto(int fd, const void *buf, size_t count, int flags,
    const struct sockaddr* addr, socklen_t addr_len) {
  CHECK_IS_SOCKET(fd);
  CHECK_SEND_FLAGS(flags);

  socket_t *sock = (socket_t*) __fdt[fd].io_object;
  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    struct iovec iov;
    iov.iov_base = (void*)buf;
    iov.iov_len = count;

    return __write_datagram_socket(sock, &iov, 1, addr, addr_len);

  }

  return _write_socket(sock, buf, count);
}


ssize_t recvfrom(int fd, void *buf, size_t count, int flags, struct sockaddr* addr,
    socklen_t* addr_len) {

  CHECK_IS_SOCKET(fd);
  if (flags != 0) {
    klee_warning("recvfrom() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  if (addr != NULL) {
    if (addr_len == NULL) {
      errno = EINVAL;
      return -1;
    }
  }

  socket_t *sock = (socket_t*) __fdt[fd].io_object;
  assert(sock != NULL);

  if(sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    struct iovec iov;
    iov.iov_base = (void*)buf;
    iov.iov_len = count;

    return __read_datagram_socket(sock, &iov, 1, addr, addr_len);
  }

  int res = _read_socket(sock, buf, count);
  klee_debug("recvfrom: res = %d\n", (int)res);
  return res;
}

// sendmsg()/recvmsg() /////////////////////////////////////////////////////////

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  CHECK_IS_SOCKET(sockfd);

  if (flags != 0) {
    klee_warning("sendmsg() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }
  if (msg->msg_control != NULL) {
    klee_warning("control data unsupported");
    errno = EINVAL;
    return -1;
  }

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;
  assert(sock != NULL);

  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    return __write_datagram_socket(sock, msg->msg_iov, msg->msg_iovlen, msg->msg_name, msg->msg_namelen);
  }

  return _gather_write(sockfd, msg->msg_iov, msg->msg_iovlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  klee_debug("recvmsg: fd = %d\n", sockfd);
  CHECK_IS_SOCKET(sockfd);

  if (flags != 0) {
    klee_warning("flags unsupported for now");
    errno = EINVAL;
    return -1;
  }
  if (msg->msg_control != NULL) {
    klee_warning("control data unsupported");
    errno = EINVAL;
    return -1;
  }

  msg->msg_flags = 0;

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;
  assert(sock != NULL);

  if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
    return __read_datagram_socket(sock, msg->msg_iov, msg->msg_iovlen, msg->msg_name, &msg->msg_namelen);
  }

  return _scatter_read(sockfd, msg->msg_iov, msg->msg_iovlen);
}

// {get,set}sockopt() //////////////////////////////////////////////////////////

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*) __fdt[sockfd].io_object;

  switch (level) {
  case SOL_SOCKET:
    switch (optname) {
    case SO_ACCEPTCONN:
      if (*optlen < sizeof(int)) {
        errno = EINVAL;
        return -1;
      }

      *((int*) optval) = (sock->status == SOCK_STATUS_LISTENING);
      *optlen = sizeof(int);
      break;
    case SO_ERROR:
      if (*optlen < sizeof(int)) {
        errno = EINVAL;
        return -1;
      }
      // XXX: We currently do not support any of the possible socket errors
      *((int*) optval) = 0;
      *optlen = sizeof(int);
      break;
    default:
      klee_warning("unsupported optname");
      errno = EINVAL;
      return -1;
    }
    break;
  default:
    klee_warning("unsupported level");
    errno = EINVAL;
    return -1;
  }

  return 0;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
  CHECK_IS_SOCKET(sockfd);

  switch (level) {
  case SOL_SOCKET:
    // Just silently accept all options
    break;
  case IPPROTO_IP:
    break;
  case IPPROTO_TCP:
    break;
  default:
    klee_warning("unsupported level");
    errno = EINVAL;
    return -1;
  }

  return 0;
}

// socketpair() ////////////////////////////////////////////////////////////////

int socketpair(int domain, int type, int protocol, int sv[2]) {
  klee_debug("Attempting to create a pair of UNIX sockets\n");
  int base_type = type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK);

  // Check the parameters
  if (domain != AF_UNIX) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  if (base_type != SOCK_STREAM) {
    klee_warning("unsupported socketpair() type");
    errno = EINVAL;
    return -1;
  }

  if (protocol != 0 && protocol != IPPROTO_TCP) {
    klee_warning("unsupported socketpair() protocol");
    errno = EPROTONOSUPPORT;
    return -1;
  }

  if (INJECT_FAULT(socketpair, EMFILE, ENFILE)) {
    return -1;
  }

  // Get two file descriptors
  int fd1, fd2;

  STATIC_LIST_ALLOC(__fdt, fd1);
  if (fd1 == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  STATIC_LIST_ALLOC(__fdt, fd2);
  if (fd2 == MAX_FDS) {
    STATIC_LIST_CLEAR(__fdt, fd1);

    errno = ENFILE;
    return -1;
  }

  // Get two anonymous UNIX end points
  end_point_t *ep1, *ep2;

  ep1 = __get_unix_end(NULL);
  ep2 = __get_unix_end(NULL);

  if (!ep1 || !ep2) {
    STATIC_LIST_CLEAR(__fdt, fd1);
    STATIC_LIST_CLEAR(__fdt, fd2);

    if (ep1)
      __release_end_point(ep1);

    if (ep2)
      __release_end_point(ep2);

    errno = ENOMEM;
    return -1;
  }

  // Create the first socket object
  socket_t *sock1 = (socket_t*) malloc(sizeof(socket_t));
  klee_make_shared(sock1, sizeof(socket_t));
  memset(sock1, 0, sizeof(socket_t));

  __fdt[fd1].attr |= FD_IS_SOCKET;
  __fdt[fd1].io_object = (file_base_t*) sock1;

  sock1->__bdata.flags |= O_RDWR;
  sock1->__bdata.refcount = 1;
  sock1->__bdata.queued = 0;

  sock1->domain = domain;
  sock1->type = base_type;
  sock1->status = SOCK_STATUS_CONNECTED;

  sock1->local_end = ep1;
  ep1->socket = sock1;

  sock1->remote_end = ep2;
  sock1->remote_end->refcount++;

  sock1->in = _stream_create(STREAM_BUFFER_SIZE, 1);
  sock1->out = _stream_create(STREAM_BUFFER_SIZE, 1);

  // Create the second socket object
  socket_t *sock2 = (socket_t*) malloc(sizeof(socket_t));
  klee_make_shared(sock2, sizeof(socket_t));
  memset(sock2, 0, sizeof(socket_t));

  __fdt[fd2].attr |= FD_IS_SOCKET;
  __fdt[fd2].io_object = (file_base_t*) sock2;

  sock2->__bdata.flags |= O_RDWR;
  sock2->__bdata.refcount = 1;
  sock2->__bdata.queued = 0;

  sock2->domain = domain;
  sock2->type = base_type;
  sock2->status = SOCK_STATUS_CONNECTED;

  sock2->local_end = ep2;
  ep2->socket = sock2;

  sock2->remote_end = ep1;
  sock2->remote_end->refcount++;

  sock2->in = sock1->out;
  sock2->out = sock1->in;

  if (type & SOCK_CLOEXEC) {
    __fdt[fd1].attr |= FD_CLOSE_ON_EXEC;
    __fdt[fd2].attr |= FD_CLOSE_ON_EXEC;
  }

  if (type & SOCK_NONBLOCK) {
    sock1->__bdata.flags |= O_NONBLOCK;
    sock2->__bdata.flags |= O_NONBLOCK;
  }

  // Finalize
  sv[0] = fd1;
  sv[1] = fd2;
  klee_debug("Socket pair created (%d, %d)\n", fd1, fd2);
  return 0;
}

// getaddrinfo() & related /////////////////////////////////////////////////////

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
    struct addrinfo **res) {
  if (node == NULL && service == NULL) {
    return EAI_NONAME;
  }

  if (hints) {
    if (hints->ai_family != AF_INET && hints->ai_family != AF_UNSPEC) {
      klee_warning("unsupported family (EAI_ADDRFAMILY)");
      return EAI_ADDRFAMILY;
    }

    if (hints->ai_socktype != 0 && hints->ai_socktype != SOCK_STREAM
        && hints->ai_socktype != SOCK_DGRAM) {
      klee_warning("unsupported socket type (EAI_SOCKTYPE)");
      return EAI_SOCKTYPE;
    }

    if (hints->ai_protocol != 0
        && hints->ai_protocol != IPPROTO_TCP
        && hints->ai_protocol != IPPROTO_UDP) {
      klee_warning("unsupported protocol (EAI_SERVICE)");
      return EAI_SERVICE;
    }

    // We kinda ignore all the flags, they don't make sense given the
    // current limitations of the model.


    if (hints->ai_addr || hints->ai_addrlen || hints->ai_canonname || hints->ai_next) {
      return EAI_SYSTEM;
    }
  }

  int port = 0;

  if (service != NULL) {
    klee_debug("Getting the port number for service '%s'\n", service);
    port = atoi(service);
    if (port == 0 && strcmp(service, "0") != 0) {
      klee_warning("service name not numeric, unsupported by model");
      return EAI_SERVICE;
    }

    if (port < 0 || port > 65535) {
      return EAI_SERVICE;
    }
  }

  if (node != NULL) {
    if (strcmp(node, "localhost") != 0 && strcmp(node, DEFAULT_HOST_NAME) != 0) {
      klee_debug("resolving '%s' to localhost\n", node);
    }
  }

  struct addrinfo *info = (struct addrinfo*) malloc(sizeof(struct addrinfo));
  memset(info, 0, sizeof(struct addrinfo));

  struct sockaddr_in *addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
  memset(addr, 0, sizeof(struct sockaddr_in));

  info->ai_addr = (struct sockaddr*) addr;
  info->ai_addrlen = sizeof(struct sockaddr_in);
  info->ai_family = AF_INET;
  info->ai_protocol = 0;
  info->ai_socktype = hints->ai_socktype;

  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = htonl(DEFAULT_NETWORK_ADDR);
  if (port != 0)
    addr->sin_port = htons((uint16_t) port);

  *res = info;
  return 0;
}

void freeaddrinfo(struct addrinfo *res) {
  if (res) {
    if (res->ai_addr) {
      free(res->ai_addr);
    }

    free(res);
  }
}

// getnameinfo() ///////////////////////////////////////////////////////////////

#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ > 13)
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
    char *serv, socklen_t servlen, int flags) {
#else
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
    char *serv, socklen_t servlen, unsigned int flags) {
#endif

  if (sa->sa_family != AF_INET || salen < sizeof(struct sockaddr_in)) {
    return EAI_FAMILY;
  }

  const struct sockaddr_in *addr = (struct sockaddr_in*) sa;

  if (host != NULL) {
    if (addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK) && addr->sin_addr.s_addr
        != __net.net_addr.s_addr) {
      klee_warning("foreign IP address solved to local network");
    }

    strncpy(host, DEFAULT_HOST_NAME, hostlen);
  }

  if (serv != NULL) {
    snprintf(serv, servlen, "%d", ntohs(addr->sin_port));
  }

  return 0;
}

// Byte order conversion ///////////////////////////////////////////////////////

#undef htons
#undef htonl
#undef ntohs
#undef ntohl

uint16_t htons(uint16_t v) {
  return (v >> 8) | (v << 8);
}

uint32_t htonl(uint32_t v) {
  return htons(v >> 16) | (htons((uint16_t) v) << 16);
}

uint16_t ntohs(uint16_t v) {
  return htons(v);
}

uint32_t ntohl(uint32_t v) {
  return htonl(v);
}
