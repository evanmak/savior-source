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
#include "files.h"
#include "sockets.h"

#include "models.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <klee/klee.h>

// File descriptor table
fd_entry_t __fdt[MAX_FDS];

// Symbolic file system
filesystem_t    __fs;
disk_file_t __stdin_file;

// Symbolic network
network_t       __net;
unix_t          __unix_net;
netlink_t       __netlink_net;

static void _init_fdt(void) {
  STATIC_LIST_INIT(__fdt);

  int fd;

  fd = _open_symbolic(&__stdin_file, O_RDONLY, 0);
  if (fd == -1)
    klee_silent_exit(0);
  assert(fd == 0);

  fd = _open_concrete(1, O_WRONLY);
  assert(fd == 1);

  fd = _open_concrete(1, O_WRONLY);
  assert(fd == 2);
}

static void _init_symfiles(unsigned n_files, unsigned file_length) {
  char fname[] = "FILE??";
  unsigned int fname_len = strlen(fname);

  struct stat s;
  int res = CALL_UNDERLYING(stat, ".", &s);

  assert(res == 0 && "Could not get default stat values");

  klee_make_shared(&__fs, sizeof(filesystem_t));
  memset(&__fs, 0, sizeof(filesystem_t));

  __fs.count = n_files;
  __fs.files = (disk_file_t**)malloc(n_files*sizeof(disk_file_t*));
  klee_make_shared(__fs.files, n_files*sizeof(disk_file_t*));

  // Create n symbolic files
  unsigned int i;
  for (i = 0; i < n_files; i++) {
    __fs.files[i] = (disk_file_t*)malloc(sizeof(disk_file_t));
    klee_make_shared(__fs.files[i], sizeof(disk_file_t));

    disk_file_t *dfile = __fs.files[i];

    fname[fname_len-1] = '0' + (i % 10);
    fname[fname_len-2] = '0' + (i / 10);

    __init_disk_file(dfile, file_length, fname, &s, 1);
  }

  // Create the stdin symbolic file
  klee_make_shared(&__stdin_file, sizeof(disk_file_t));
  __init_disk_file(&__stdin_file, MAX_STDINSIZE, "STDIN", &s, 1);
}

static void _init_network(void) {
  // Initialize the INET domain
  klee_make_shared(&__net, sizeof(__net));

  __net.net_addr.s_addr = htonl(DEFAULT_NETWORK_ADDR);
  __net.next_port = htons(DEFAULT_UNUSED_PORT);
  STATIC_LIST_INIT(__net.end_points);

  // Initialize the UNIX domain
  klee_make_shared(&__unix_net, sizeof(__unix_net));
  STATIC_LIST_INIT(__unix_net.end_points);

  // Initialize the netlink domain
  klee_make_shared(&__netlink_net, sizeof(__netlink_net));
  STATIC_LIST_INIT(__netlink_net.end_points);
}

void klee_init_fds(unsigned n_files, unsigned file_length, char unsafe,
    char overlapped) {
  _init_symfiles(n_files, file_length);
  _init_network();

  _init_fdt();

  // Setting the unsafe (allow external writes) flag
  __fs.unsafe = unsafe;
  // Setting the overlapped (keep per-state concrete file offsets) flag
  __fs.overlapped = overlapped;
}
