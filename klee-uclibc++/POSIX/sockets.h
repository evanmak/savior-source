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

#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "fd.h"
#include "buffers.h"
#include "multiprocess.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define DEFAULT_UNUSED_PORT     32768
#define DEFAULT_NETWORK_ADDR    ((((((192 << 8) | 168) << 8) | 1) << 8) | 1) // 192.168.1.1
#define DEFAULT_HOST_NAME       "192.168.1.1"

#define SOCK_STATUS_CREATED         (1 << 0)
#define SOCK_STATUS_LISTENING       (1 << 1)
#define SOCK_STATUS_CONNECTING      (1 << 2)
#define SOCK_STATUS_CONNECTED       (1 << 3)
#define SOCK_STATUS_CLOSING         (1 << 4) // Transient state due to concurrency

struct socket;

typedef struct {
  struct sockaddr *addr;

  struct socket *socket;

  unsigned int refcount;
  char allocated;
}  end_point_t;

typedef struct {
  // For TCP/IP sockets
  struct in_addr net_addr;  // The IP address of the virtual machine

  in_port_t next_port;

  end_point_t end_points[MAX_PORTS];
} network_t;

typedef struct {
  end_point_t end_points[MAX_UNIX_EPOINTS];
} unix_t;

typedef struct {
  end_point_t end_points[MAX_NETLINK_EPOINTS];
} netlink_t;

extern network_t __net;
extern unix_t __unix_net;
extern netlink_t __netlink_net;

typedef struct {
  struct sockaddr* src;
  size_t src_len;

  block_buffer_t contents;
} datagram_t;

typedef struct socket {
  file_base_t __bdata;

  int status;
  int type;
  int domain;
  int protocol;

  end_point_t *local_end;
  end_point_t *remote_end;

  // For TCP connections
  stream_buffer_t *out;     // The output buffer
  stream_buffer_t *in;      // The input buffer

  // For connecting sockets
  event_queue_t *conn_evt_queue;
  wlist_id_t conn_wlist;         // The waiting list for the connected notif.

  // For TCP listening
  stream_buffer_t *listen;
} socket_t;

int _close_socket(socket_t *sock);
ssize_t _read_socket(socket_t *sock, void *buf, size_t count);
ssize_t _write_socket(socket_t *sock, const void *buf, size_t count);
int _stat_socket(socket_t *sock, struct stat *buf);
int _ioctl_socket(socket_t *sock, unsigned long request, char *argp);

int _is_blocking_socket(socket_t *sock, int event);
int _register_events_socket(socket_t *sock, wlist_id_t wlist, int events);
void _deregister_events_socket(socket_t *sock, wlist_id_t wlist, int events);


#endif /* SOCKETS_H_ */
