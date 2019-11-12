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

#include "files.h"

#include "common.h"
#include "models.h"

#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <klee/klee.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/syscall.h>

#ifdef _FNC
#include "fnc_redhat.h"
#endif

// __NR_lseek was removed in ubuntu 11.04
#ifdef __NR3264_lseek
#define __NR_lseek __NR3264_lseek
#endif


#define CHECK_IS_FILE(fd) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) { \
    errno = EBADF; \
    return -1; \
    } \
    if (!(__fdt[fd].attr & FD_IS_FILE)) { \
      errno = ESPIPE; \
      return -1; \
    } \
  } while (0)


////////////////////////////////////////////////////////////////////////////////
// Internal File System Routines
////////////////////////////////////////////////////////////////////////////////

static int __isupper(const char c) {
  return (('A' <= c) & (c <= 'Z'));
}

/* Returns pointer to the symbolic file structure if the pathname is symbolic */
disk_file_t *__get_sym_file(const char *pathname) {
  char c = pathname[0];

  if (c == 0 || pathname[1] != 0)
    return NULL;

  unsigned int i;
  for (i = 0; i < __fs.count; i++) {
    if (c == 'A' + (char)i) {
      disk_file_t *df = __fs.files[i];
      return df;
    }
  }

  return NULL;
}

static void _init_stats(disk_file_t *dfile, const struct stat *defstats) {
  struct stat *stat = dfile->stat;

  /* Important since we copy this out through getdents, and readdir
     will otherwise skip this entry. For same reason need to make sure
     it fits in low bits. */
  klee_assume((stat->st_ino & 0x7FFFFFFF) != 0);

  /* uclibc opendir uses this as its buffer size, try to keep
     reasonable. */
  klee_assume((stat->st_blksize & ~0xFFFF) == 0);

  klee_prefer_cex(stat, !(stat->st_mode & ~(S_IFMT | 0777)));
  klee_prefer_cex(stat, stat->st_dev == defstats->st_dev);
  klee_prefer_cex(stat, stat->st_rdev == defstats->st_rdev);
  klee_prefer_cex(stat, (stat->st_mode&0700) == 0600);
  klee_prefer_cex(stat, (stat->st_mode&0070) == 0020);
  klee_prefer_cex(stat, (stat->st_mode&0007) == 0002);
  klee_prefer_cex(stat, (stat->st_mode&S_IFMT) == S_IFREG);
  klee_prefer_cex(stat, stat->st_nlink == 1);
  klee_prefer_cex(stat, stat->st_uid == defstats->st_uid);
  klee_prefer_cex(stat, stat->st_gid == defstats->st_gid);
  klee_prefer_cex(stat, stat->st_blksize == 4096);
  klee_prefer_cex(stat, stat->st_atime == defstats->st_atime);
  klee_prefer_cex(stat, stat->st_mtime == defstats->st_mtime);
  klee_prefer_cex(stat, stat->st_ctime == defstats->st_ctime);

  stat->st_size = dfile->contents.size;
  stat->st_blocks = 8;
}

void __init_disk_file(disk_file_t *dfile, size_t maxsize, const char *symname,
    const struct stat *defstats, int symstats) {
  char namebuf[64];

  // Initializing the file name...
  dfile->name = (char*)malloc(MAX_PATH_LEN);

  strcpy(namebuf, symname); strcat(namebuf, "-name");
  klee_make_symbolic(dfile->name, MAX_PATH_LEN, namebuf);
  klee_make_shared(dfile->name, MAX_PATH_LEN);

  unsigned int i;
  for (i = 0; i < MAX_PATH_LEN; i++) {
    klee_prefer_cex(dfile->name, __isupper(dfile->name[i]));
  }

  // Initializing the buffer contents...
  _block_init(&dfile->contents, maxsize);
  dfile->contents.size = maxsize;
  strcpy(namebuf, symname); strcat(namebuf, "-data");
  klee_make_symbolic(dfile->contents.contents, maxsize, namebuf);
  klee_make_shared(dfile->contents.contents, maxsize);

  // Initializing the statistics...
  dfile->stat = (struct stat*)malloc(sizeof(struct stat));

  if (symstats) {
    strcpy(namebuf, symname); strcat(namebuf, "-stat");
    klee_make_symbolic(dfile->stat, sizeof(struct stat), namebuf);
    klee_make_shared(dfile->stat, sizeof(struct stat));
    _init_stats(dfile, defstats);
  } else {
    memcpy(dfile->stat, defstats, sizeof(struct stat));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal File Routines
////////////////////////////////////////////////////////////////////////////////

static int __is_concrete_blocking(int concretefd, int event) {
  fd_set fds;
  _FD_ZERO(&fds);
  _FD_SET(concretefd, &fds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  int res;

  switch(event) {
  case EVENT_READ:
    res = CALL_UNDERLYING(select, concretefd+1, &fds, NULL, NULL, &timeout);
    return (res == 0);
  case EVENT_WRITE:
    res = CALL_UNDERLYING(select, concretefd+1, NULL, &fds, NULL, &timeout);
    return (res == 0);
  default:
      assert(0 && "invalid event");
  }
}

int _is_blocking_file(file_t *file, int event) {
  if (_file_is_concrete(file))
    return __is_concrete_blocking(file->concrete_fd, event);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////

ssize_t _read_file(file_t *file, void *buf, size_t count, off_t offset) {
  if (_file_is_concrete(file)) {
    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
      before concretization, at least once the routine has been fixed
      to properly work with symbolics. */
    klee_check_memory_access(buf, count);

    int res;

    if (offset >= 0)
      res = CALL_UNDERLYING(pread, file->concrete_fd, buf, count, offset);
    else if (file->offset >= 0)
      res = CALL_UNDERLYING(pread, file->concrete_fd, buf, count, file->offset);
    else
      res = CALL_UNDERLYING(read, file->concrete_fd, buf, count);

    if (res == -1)
     errno = klee_get_errno();
    else {
      if (file->offset >= 0 && offset < 0)
        file->offset += res;
    }

    return res;
  }

  ssize_t res = _block_read(&file->storage->contents, buf, count,
      offset >= 0 ? offset : file->offset);

  if (res < 0) {
    errno = EINVAL;
    return -1;
  }

  if (offset < 0)
    file->offset += res;

  return res;
}

ssize_t _write_file(file_t *file, const void *buf, size_t count, off_t offset) {
  if (_file_is_concrete(file)) {
    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
      before concretization, at least once the routine has been fixed
      to properly work with symbolics. */
    klee_check_memory_access(buf, count);

    int res;

    //klee_debug("Writing concretely at (%d) %d bytes...\n", file->concrete_fd, count);

    if (offset >= 0)
      res = CALL_UNDERLYING(pwrite, file->concrete_fd, buf, count, offset);
    else if (file->offset >= 0)
      res = CALL_UNDERLYING(pwrite, file->concrete_fd, buf, count, file->offset);
    else
      res = CALL_UNDERLYING(write, file->concrete_fd, buf, count);

    if (res == -1)
      errno = klee_get_errno();
    else {
      if (file->offset >= 0 && offset < 0)
        file->offset += res;
    }

    return res;
  }

  //klee_debug("Writing symbolically %d bytes...\n", count);

  ssize_t res = _block_write(&file->storage->contents, buf, count,
      offset >= 0 ? offset : file->offset);

  if (res < 0) {
    errno = EINVAL;
    return -1;
  }

  if (offset < 0) {
    file->offset += res;
    file->storage->stat->st_size = file->storage->contents.size;
  }

  if (res == 0)
    errno = EFBIG;

  return res;
}

////////////////////////////////////////////////////////////////////////////////

static int _stat_dfile(disk_file_t *dfile, struct stat *buf) {
  if (INJECT_FAULT(fstat, ELOOP, ENOMEM)) {
    return -1;
  }

  memcpy(buf, dfile->stat, sizeof(struct stat));
  return 0;
}

int _stat_file(file_t *file, struct stat *buf) {
  if (_file_is_concrete(file)) {
    int res = CALL_UNDERLYING(fstat, file->concrete_fd, buf);

    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return _stat_dfile(file->storage, buf);
}

DEFINE_MODEL(int, stat, const char *path, struct stat *buf) {
  disk_file_t *dfile = __get_sym_file(path);

  if (!dfile) {
    int res = CALL_UNDERLYING(stat, __concretize_string(path), buf);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return _stat_dfile(dfile, buf);
}

#if 0
DEFINE_MODEL(int, fstatat, int dirfd, const char *pathname, struct stat *buf,
                   int flags) {
  disk_file_t *dfile = __get_sym_file(pathname);

  if (!dfile) {
    int concrete_dirfd = __get_concrete_fd(dirfd);
    if (concrete_dirfd < 0)
      return -1;

    int res = CALL_UNDERLYING(fstatat, concrete_dirfd,
        __concretize_string(pathname), buf, flags);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return _stat_dfile(dfile, buf);
}
#endif

DEFINE_MODEL(int, lstat, const char *path, struct stat *buf) {
  disk_file_t *dfile = __get_sym_file(path);

  if (!dfile) {
    int res = CALL_UNDERLYING(lstat, __concretize_string(path), buf);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return _stat_dfile(dfile, buf);
}

////////////////////////////////////////////////////////////////////////////////

int _ioctl_file(file_t *file, unsigned long request, char *argp) {
  // For now, this works only on concrete FDs
  if (_file_is_concrete(file)) {
    int res = CALL_UNDERLYING(ioctl, file->concrete_fd, request, argp);
    if (res == -1) {
      errno = klee_get_errno();
    }
    return res;
  }

  klee_warning("operation not supported on symbolic files");
  errno = ENOTTY;
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

/* Returns 1 if the process has the access rights specified by 'flags'
   to the file with stat 's'.  Returns 0 otherwise*/
static int _can_open(int flags, const struct stat *s) {
  int write_access, read_access;
  mode_t mode = s->st_mode;

  if ((flags & O_ACCMODE) != O_WRONLY)
    read_access = 1;
  else
    read_access = 0;

  if ((flags & O_ACCMODE) != O_RDONLY)
    write_access = 1;
  else
    write_access = 0;

  /* XXX: We don't worry about process uid and gid for now.
     We allow access if any user has access to the file. */
#if 0
  uid_t uid = s->st_uid;
  uid_t euid = geteuid();
  gid_t gid = s->st_gid;
  gid_t egid = getegid();
#endif

  if (read_access && !(mode & (S_IRUSR | S_IRGRP | S_IROTH)))
    return 0;

  if (write_access && !(mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
    return 0;

  return 1;
}

DEFINE_MODEL(int, open, const char *pathname, int flags, ...) {
  mode_t mode = 0;

  if (flags & O_CREAT) {
    /* get mode */
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  if (INJECT_FAULT(open, EINTR, ELOOP, EMFILE, ENFILE, ENOMEM, EPERM)) {
    return -1;
  }

  klee_debug("Attempting to open: %s\n", __concretize_string(pathname));

  // Obtain a symbolic file
  disk_file_t *dfile = __get_sym_file(pathname);

  if (dfile) {
    return _open_symbolic(dfile, flags, mode);
  } else {
    if ((flags & O_ACCMODE) != O_RDONLY && !__fs.unsafe) {
      klee_warning("blocked non-r/o access to concrete file");
      errno = EACCES;
      return -1;
    }

    int concrete_fd = CALL_UNDERLYING(open, __concretize_string(pathname),
        flags, mode);

    if (concrete_fd == -1) {
      errno = klee_get_errno();
      return -1;
    }

    int fd = _open_concrete(concrete_fd, flags);

    if (fd == -1) {
      CALL_UNDERLYING(close, concrete_fd);
      return -1;
    }

    return fd;
  }
}

int _open_concrete(int concrete_fd, int flags) {
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);

  if (fd == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];
  fde->attr |= FD_IS_FILE;

  file_t *file = (file_t*)malloc(sizeof(file_t));
  klee_make_shared(file, sizeof(file_t));
  memset(file, 0, sizeof(file_t));

  file->__bdata.flags = flags;
  file->__bdata.refcount = 1;
  file->storage = NULL;
  file->concrete_fd = concrete_fd;

  fde->io_object = (file_base_t*)file;

  // Check to see if the concrete FD is a char/PIPE/socket
  struct stat s;
  int res = CALL_UNDERLYING(fstat, concrete_fd, &s);
  assert(res == 0);

  if ((S_ISREG(s.st_mode) && !__fs.overlapped) || (S_ISCHR(s.st_mode) || S_ISFIFO(s.st_mode) ||
      S_ISSOCK(s.st_mode))) {
    file->offset = -1;
  } else {
    file->offset = CALL_UNDERLYING(lseek, concrete_fd, 0, SEEK_CUR);
    assert(file->offset >= 0);
  }

  if (flags & O_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  }

  return fd;
}

int _open_symbolic(disk_file_t *dfile, int flags, mode_t mode) {
  // Checking the flags
  if ((flags & O_CREAT) && (flags & O_EXCL)) {
    errno = EEXIST;
    return -1;
  }

  if ((flags & O_TRUNC) && (flags & O_ACCMODE) == O_RDONLY) {
    /* The result of using O_TRUNC with O_RDONLY is undefined, so we
   return error */
    klee_warning("Undefined call to open(): O_TRUNC | O_RDONLY\n");
    errno = EACCES;
    return -1;
  }

  if ((flags & O_EXCL) && !(flags & O_CREAT)) {
    /* The result of using O_EXCL without O_CREAT is undefined, so
   we return error */
    klee_warning("Undefined call to open(): O_EXCL w/o O_RDONLY\n");
    errno = EACCES;
    return -1;
  }

  if (!_can_open(flags, dfile->stat)) {
    errno = EACCES;
    return -1;
  }

  // Now we can allocate a FD
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);

  if (fd == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];
  fde->attr |= FD_IS_FILE;

  // Now we can set up the open file structure...
  file_t *file = (file_t*)malloc(sizeof(file_t));
  klee_make_shared(file, sizeof(file_t));
  memset(file, 0, sizeof(file_t));

  file->__bdata.flags = flags;
  file->__bdata.refcount = 1;
  file->storage = dfile;
  file->offset = 0;
  file->concrete_fd = -1;

  if ((flags & O_ACCMODE) != O_RDONLY && (flags & O_TRUNC)) {
    file->storage->contents.size = 0;
  }

  if (flags & O_APPEND) {
    file->offset = file->storage->contents.size;
  }

  if (flags & O_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  }

  fde->io_object = (file_base_t*)file;

  return fd;
}

DEFINE_MODEL(int, creat, const char *pathname, mode_t mode) {
  return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

////////////////////////////////////////////////////////////////////////////////

int _close_file(file_t *file) {
  if (INJECT_FAULT(close, EIO))
    return -1;

  free(file);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(char *, getcwd, char *buf, size_t size) {
  char *r;

  if (!buf) {
    if (!size)
      size = 1024;
    buf = malloc(size);
  }

  buf = __concretize_ptr(buf);
  size = __concretize_size(size);
  /* XXX In terms of looking for bugs we really should do this check
     before concretization, at least once the routine has been fixed
     to properly work with symbolics. */
  klee_check_memory_access(buf, size);
  r = CALL_UNDERLYING(getcwd, buf, size);

  if (r == NULL) {
    errno = klee_get_errno();
    return NULL;
  }

  return buf;
}

////////////////////////////////////////////////////////////////////////////////

static off_t _lseek(file_t *file, off_t offset, int whence) {
  off_t newOff;
  switch (whence) {
  case SEEK_SET:
    newOff = offset;
    break;
  case SEEK_CUR:
    newOff = file->offset + offset;
    break;
  case SEEK_END:
    newOff = file->storage->contents.size + offset;
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  if (newOff < 0 || (size_t)newOff > file->storage->contents.size) {
    errno = EINVAL;
    return -1;
  }

  file->offset = newOff;
  return file->offset;
}

DEFINE_MODEL(off_t, lseek, int fd, off_t offset, int whence) {
  CHECK_IS_FILE(fd);

  file_t *file = (file_t*)__fdt[fd].io_object;

  if (_file_is_concrete(file)) {
    int res = CALL_UNDERLYING(lseek, file->concrete_fd, offset, whence);
    if (res == -1) {
      errno = klee_get_errno();
    } else {
      assert(file->offset >= 0);
      file->offset = res;
    }

    return res;
  }

  return _lseek(file, offset, whence);
}

////////////////////////////////////////////////////////////////////////////////

static int _chmod(disk_file_t *dfile, mode_t mode) {
  if (geteuid() == dfile->stat->st_uid) {
    if (getgid() != dfile->stat->st_gid)
      mode &= ~ S_ISGID;
    dfile->stat->st_mode = ((dfile->stat->st_mode & ~07777) |
                         (mode & 07777));
    return 0;
  } else {
    errno = EPERM;
    return -1;
  }
}

DEFINE_MODEL(int, chmod, const char *path, mode_t mode) {
  disk_file_t *dfile = __get_sym_file(path);

  if (!dfile) {
    int res = CALL_UNDERLYING(chmod, __concretize_string(path), mode);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return _chmod(dfile, mode);
}

DEFINE_MODEL(int, fchmod, int fd, mode_t mode) {
  CHECK_IS_FILE(fd);

  file_t *file = (file_t*)__fdt[fd].io_object;

  if (_file_is_concrete(file)) {
    int res = CALL_UNDERLYING(fchmod, file->concrete_fd, mode);

    if (res == -1)
      errno = klee_get_errno();

    return res;
  }

  return _chmod(file->storage, mode);
}


////////////////////////////////////////////////////////////////////////////////
// Directory management
////////////////////////////////////////////////////////////////////////////////

int getdents(unsigned int fd, struct dirent64 *dirp, unsigned int count) {
  CHECK_IS_FILE(fd);

  file_t *file = (file_t*)__fdt[fd].io_object;

  if (!_file_is_concrete(file)) {
    klee_warning("symbolic file, ignoring (EINVAL)");
    errno = EINVAL;
    return -1;
  }

  if ((unsigned long) file->offset < 4096u) {
    /* Return our dirents */
    unsigned i, pad, bytes=0;

    /* What happens for bad offsets? */
    i = file->offset / sizeof(*dirp);
    if (((off64_t) (i * sizeof(*dirp)) != file->offset) ||
        i > __fs.count) {
      errno = EINVAL;
      return -1;
    }

    for (; i< __fs.count; ++i) {
      disk_file_t *df = __fs.files[i];

      dirp->d_ino = df->stat->st_ino;
      dirp->d_reclen = sizeof(*dirp);
      dirp->d_type = IFTODT(df->stat->st_mode);
      dirp->d_name[0] = 'A' + i;
      dirp->d_name[1] = '\0';
      dirp->d_off = (i+1) * sizeof(*dirp);
      bytes += dirp->d_reclen;
      ++dirp;
    }

    /* Fake jump to OS records by a "deleted" file. */
    pad = count>=4096 ? 4096 : count;
    dirp->d_ino = 0;
    dirp->d_reclen = pad - bytes;
    dirp->d_type = DT_UNKNOWN;
    dirp->d_name[0] = '\0';
    dirp->d_off = 4096;
    bytes += dirp->d_reclen;
    file->offset = pad;
    return bytes;
  } else {
    unsigned os_pos = file->offset - 4096;
    int res, s;

    /* For reasons which I really don't understand, if I don't
       memset this then sometimes the kernel returns d_ino==0 for
       some valid entries? Am I crazy? Can writeback possibly be
       failing?

       Even more bizarre, interchanging the memset and the seek also
       case strange behavior. Really should be debugged properly. */
    memset(dirp, 0, count);
    s = syscall(__NR_lseek, file->concrete_fd, (int) os_pos, SEEK_SET);
    assert(s != (off64_t) -1);
    res = syscall(__NR_getdents64, file->concrete_fd, dirp, count);
    if (res == -1) {
      errno = klee_get_errno();
    } else {
      int pos = 0;

      file->offset = syscall(__NR_lseek, file->concrete_fd, 0, SEEK_CUR) + 4096;

      /* Patch offsets */

      while (pos < res) {
        struct dirent64 *dp = (struct dirent64*) ((char*) dirp + pos);
        dp->d_off += 4096;
        pos += dp->d_reclen;
      }
    }
    return res;
  }
}


int __getdents(unsigned int fd, struct dirent *dirp, unsigned int count)
     __attribute__((alias("getdents")));

int getdents64(unsigned int fd, struct dirent *dirp, unsigned int count) {
  return getdents(fd, (struct dirent64*) dirp, count);
}

int __getdents64(unsigned int fd, struct dirent *dirp, unsigned int count)
     __attribute__((alias("getdents64")));

////////////////////////////////////////////////////////////////////////////////
// Forwarded / unsupported calls
////////////////////////////////////////////////////////////////////////////////

int __xstat(int ver, const char * path, struct stat * stat_buf) {
  assert(ver == 1);
  return syscall(__NR_stat, path, (struct kernel_stat *) stat_buf);
}

int __lxstat(int ver, const char * path, struct stat * stat_buf) {
  assert(ver == 1);
  return syscall(__NR_lstat, path, (struct kernel_stat *) stat_buf);
}

int __fxstat(int ver, int fildes, struct stat * stat_buf) {
  assert(ver == 1);
  return syscall(__NR_fstat, fildes, (struct kernel_stat *) stat_buf);
}

////////////////////////////////////////////////////////////////////////////////

#define _WRAP_FILE_SYSCALL_ERROR(call, ...) \
  do { \
    if (__get_sym_file(pathname)) { \
      klee_warning("symbolic path, " #call " unsupported (ENOENT)"); \
      errno = ENOENT; \
      return -1; \
    } \
    int ret = CALL_UNDERLYING(call, __concretize_string(pathname), ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FILE_SYSCALL_IGNORE(call, ...) \
  do { \
    if (__get_sym_file(pathname)) { \
      klee_warning("symbolic path, " #call " does nothing"); \
      return 0; \
    } \
    int ret = CALL_UNDERLYING(call, __concretize_string(pathname), ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FILE_SYSCALL_BLOCK(call, ...) \
  do { \
    klee_warning(#call " blocked (EPERM)"); \
    errno = EPERM; \
    return -1; \
  } while (0)

DEFINE_MODEL(int, rmdir, const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(rmdir);
}

DEFINE_MODEL(ssize_t, readlink, const char *pathname, char *buf, size_t bufsize) {
  _WRAP_FILE_SYSCALL_ERROR(readlink, buf, bufsize);
}

DEFINE_MODEL(int, unlink, const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(unlink);
}

DEFINE_MODEL(int, chroot, const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(chroot);
}

DEFINE_MODEL(int, chown, const char *pathname, uid_t owner, gid_t group) {
  _WRAP_FILE_SYSCALL_ERROR(chown, owner, group);
}

DEFINE_MODEL(int, lchown, const char *pathname, uid_t owner, gid_t group) {
  _WRAP_FILE_SYSCALL_ERROR(lchown, owner, group);
}

DEFINE_MODEL(int, chdir, const char *pathname) {
  _WRAP_FILE_SYSCALL_ERROR(chdir);
}

////////////////////////////////////////////////////////////////////////////////

#define _WRAP_FD_SYSCALL_ERROR(call, ...) \
  do { \
    CHECK_IS_FILE(fd); \
    file_t *file = (file_t*)__fdt[fd].io_object; \
    if (!_file_is_concrete(file)) { \
      klee_warning("symbolic file, " #call " unsupported (EBADF)"); \
      errno = EBADF; \
      return -1; \
    } \
    int ret = CALL_UNDERLYING(call, file->concrete_fd, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FD_SYSCALL_IGNORE(call, ...) \
  do { \
    CHECK_IS_FILE(fd); \
    file_t *file = (file_t*)__fdt[fd].io_object; \
    if (!_file_is_concrete(file)) { \
      klee_warning("symbolic file, " #call " does nothing"); \
      return 0; \
    } \
    int ret = CALL_UNDERLYING(call, file->concrete_fd, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

DEFINE_MODEL(int, fsync, int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fsync);
}

DEFINE_MODEL(int, fdatasync, int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fdatasync);
}

DEFINE_MODEL(int, fchdir, int fd) {
  _WRAP_FD_SYSCALL_ERROR(fchdir);
}

DEFINE_MODEL(int, fchown, int fd, uid_t owner, gid_t group) {
  _WRAP_FD_SYSCALL_ERROR(fchown, owner, group);
}

DEFINE_MODEL(int, fstatfs, int fd, struct statfs *buf) {
  _WRAP_FD_SYSCALL_ERROR(fstatfs, buf);
}

DEFINE_MODEL(int, statfs, const char *pathname, struct statfs *buf) {
  _WRAP_FILE_SYSCALL_ERROR(statfs, buf);
}

DEFINE_MODEL(int, ftruncate, int fd, off_t length) {
  _WRAP_FD_SYSCALL_ERROR(ftruncate, length);
}

DEFINE_MODEL(int, truncate, const char *pathname, off_t length) {
  _WRAP_FILE_SYSCALL_ERROR(truncate, length);
}

DEFINE_MODEL(int, access, const char *pathname, int mode) {
  _WRAP_FILE_SYSCALL_IGNORE(access, mode);
}
