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

#ifndef FILES_H_
#define FILES_H_

#include "common.h"
#include "buffers.h"
#include "fd.h"

#include <sys/types.h>
#include <unistd.h>

typedef struct {
  char *name;
  block_buffer_t contents;

  struct stat *stat;
} disk_file_t;  // The "disk" storage of the file

typedef struct {
  unsigned count;
  disk_file_t **files;

  char unsafe;
  char overlapped;
} filesystem_t;

extern filesystem_t __fs;
extern disk_file_t __stdin_file;

typedef struct {
  file_base_t __bdata;

  off_t offset;

  int concrete_fd;
  disk_file_t *storage;
} file_t;       // The open file structure

void __init_disk_file(disk_file_t *dfile, size_t maxsize, const char *symname,
    const struct stat *defstats, int symstats);

disk_file_t *__get_sym_file(const char *pathname);

int _close_file(file_t *file);
ssize_t _read_file(file_t *file, void *buf, size_t count, off_t offset);
ssize_t _write_file(file_t *file, const void *buf, size_t count, off_t offset);
int _stat_file(file_t *file, struct stat *buf);
int _ioctl_file(file_t *file, unsigned long request, char *argp);

int _is_blocking_file(file_t *file, int event);

static inline int _file_is_concrete(file_t *file) {
  return file->concrete_fd >= 0;
}

int _open_concrete(int concrete_fd, int flags);
int _open_symbolic(disk_file_t *dfile, int flags, mode_t mode);


#endif /* FILES_H_ */
