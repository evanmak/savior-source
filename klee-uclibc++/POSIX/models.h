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

#ifndef UNDERLYING_H_
#define UNDERLYING_H_

#include "common.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>

DECLARE_MODEL(int, stat, const char *path, struct stat *buf);
DECLARE_MODEL(int, fstat, int fd, struct stat *buf);
DECLARE_MODEL(int, lstat, const char *path, struct stat *buf);
#if 0
DECLARE_MODEL(int, fstatat, int dirfd, const char *pathname, struct stat *buf,
                   int flags);
#endif

DECLARE_MODEL(int, close, int fd);

DECLARE_MODEL(int, fcntl, int fd, int cmd, ...);

DECLARE_MODEL(int, ioctl, int d, unsigned long request, ...);

DECLARE_MODEL(int, open, const char *pathname, int flags, ...);
DECLARE_MODEL(int, creat, const char *pathname, mode_t mode);

DECLARE_MODEL(char *, getcwd, char *buf, size_t size);

DECLARE_MODEL(off_t, lseek, int fd, off_t offset, int whence);

DECLARE_MODEL(int, chmod, const char *path, mode_t mode);
DECLARE_MODEL(int, fchmod, int fd, mode_t mode);

DECLARE_MODEL(ssize_t, readlink, const char *path, char *buf, size_t bufsiz);

DECLARE_MODEL(int, chown, const char *path, uid_t owner, gid_t group);
DECLARE_MODEL(int, fchown, int fd, uid_t owner, gid_t group);
DECLARE_MODEL(int, lchown, const char *path, uid_t owner, gid_t group);

DECLARE_MODEL(int, chdir, const char *path);
DECLARE_MODEL(int, fchdir, int fd);

DECLARE_MODEL(int, fsync, int fd);
DECLARE_MODEL(int, fdatasync, int fd);

DECLARE_MODEL(int, statfs, const char *path, struct statfs *buf);
DECLARE_MODEL(int, fstatfs, int fd, struct statfs *buf);

DECLARE_MODEL(int, truncate, const char *path, off_t length);
DECLARE_MODEL(int, ftruncate, int fd, off_t length);

DECLARE_MODEL(int, access, const char *pathname, int mode);

DECLARE_MODEL(ssize_t, read, int fd, void *buf, size_t count);
DECLARE_MODEL(ssize_t, write, int fd, const void *buf, size_t count);

DECLARE_MODEL(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt);
DECLARE_MODEL(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt);

DECLARE_MODEL(ssize_t, pread, int fd, void *buf, size_t count, off_t offset);
DECLARE_MODEL(ssize_t, pwrite, int fd, const void *buf, size_t count, off_t offset);

#ifdef HAVE_SYMBOLIC_CTYPE
DECLARE_MODEL(const int32_t **, __ctype_tolower_loc, void);
DECLARE_MODEL(const unsigned short **, __ctype_b_loc, void);
#endif

DECLARE_MODEL(int, select, int nfds, fd_set *readfds, fd_set *writefds,
    fd_set *exceptfds, struct timeval *timeout)

#endif /* UNDERLYING_H_ */
