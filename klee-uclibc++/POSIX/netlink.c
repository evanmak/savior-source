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

#include "netlink.h"

#include "sockets.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <linux/rtnetlink.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define NL_IFCOUNT    3
#define NL_IFHWSIZE   6

static char __base_hw_address[NL_IFHWSIZE] = {
    0xC1, 0x00, 0xD9, 0x00, 0x00, 0x00
};

static char __base_hw_name[] = "eth";


typedef struct netlink_iface {
  char name[IFNAMSIZ];
  char hw_addr[NL_IFHWSIZE];
  unsigned int flags;
} netlink_iface_t;

static netlink_iface_t __ifaces[NL_IFCOUNT];

static void _netlink_getlink_handler(socket_t *sock, struct nlmsghdr *nh) {

}

static void _netlink_route_handler(socket_t *sock, struct nlmsghdr *nh) {
  switch(nh->nlmsg_type) {
  case RTM_GETLINK:
    _netlink_getlink_handler(sock, nh);
    return;
  default:
    klee_debug("Unknown message received: %d\n", nh->nlmsg_type);
    klee_warning("unsupported rtnetlink message");
    break;
  }
}

void _netlink_handler(socket_t *sock, const struct iovec *iov, int iovcnt,
    size_t count) {
  void *buf;

  // Flattening the iovec structure
  if (iovcnt == 1)
    buf = iov[0].iov_base;
  else {
    buf = malloc(count);

    size_t offset = 0;
    int i;

    for (i = 0; i < iovcnt; i++) {
      memcpy(&((char*)buf)[offset], iov[i].iov_base, iov[i].iov_len);
      offset += iov[i].iov_len;
    }
  }

  struct nlmsghdr *nh;

  for (nh = (struct nlmsghdr*) buf; NLMSG_OK(nh, count);
       nh = NLMSG_NEXT(nh, count)) {

    switch (nh->nlmsg_type) {
    case NLMSG_DONE:
      return;
    case NLMSG_NOOP:
      continue;
    case NLMSG_ERROR:
      klee_warning("netlink error datagram received from user");
      return;
    }

    if (nh->nlmsg_flags & NLM_F_ACK) {
      klee_warning("netlink acknowledgments not supported");
      return;
    }

    if (nh->nlmsg_flags & NLM_F_ECHO) {
      klee_warning("netlink echoes not supported");
      return;
    }

    if (!(nh->nlmsg_flags & NLM_F_REQUEST)) {
      klee_warning("netlink request flag not set");
      return;
    }

    switch (sock->protocol) {
    case NETLINK_ROUTE:
      _netlink_route_handler(sock, nh);
      break;
    default:
      assert(0 && "invalid netlink protocol");
    }
  }

  if (iovcnt > 1)
    free(buf);
}

void klee_init_netlink(void) {
  unsigned char i;
  for (i = 0; i < NL_IFCOUNT; i++) {
    snprintf(__ifaces[i].name, IFNAMSIZ, "%s%d", __base_hw_name, i);

    memcpy(__ifaces[i].hw_addr, __base_hw_address, NL_IFHWSIZE);
    __ifaces[i].hw_addr[NL_IFHWSIZE-1] = i;

    __ifaces[i].flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING;
  }
}
