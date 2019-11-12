//===-- stubs.c -----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <utime.h>
#include <utmp.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "common.h"
#include "klee/Config/config.h"
#include "klee/klee.h"



void klee_warning(const char*);
void klee_warning_once(const char*);

#ifndef HAVE_POSIX_SIGNALS

/* Silent ignore */

__attribute__((weak))
int __syscall_rt_sigaction(int signum, const struct sigaction *act, 
                           struct sigaction *oldact, size_t _something);

int __syscall_rt_sigaction(int signum, const struct sigaction *act, 
                           struct sigaction *oldact, size_t _something) {
  klee_warning_once("silently ignoring");
  return 0;
}

__attribute__((weak))
int sigaction(int signum, const struct sigaction *act, 
              struct sigaction *oldact);

int sigaction(int signum, const struct sigaction *act, 
              struct sigaction *oldact) {
  klee_warning_once("silently ignoring");
  return 0;
}

sigset_t _sigintr; // from signal.c

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
  klee_warning_once("silently ignoring");
  return 0;
}

sighandler_t sigset(int sig, sighandler_t disp) {
  klee_warning_once("silently ignoring");
  return 0;
}

int sighold(int sig) {
  klee_warning_once("silently ignoring");
  return 0;
}

int sigrelse(int sig) {
  klee_warning_once("silently ignoring");
  return 0;
}

int sigignore(int sig) {
  klee_warning_once("silently ignoring");
  return 0;
}

unsigned int alarm(unsigned int seconds) {
  klee_warning_once("silently ignoring");
  return 0;
}

__attribute__((weak))
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  klee_warning_once("silently ignoring");
  return 0;
}

#endif /* HAVE_POSIX_SIGNALS */

/* Not even worth warning about these */
__attribute__((weak))
int fdatasync(int fd) ;
int fdatasync(int fd) {
  return 0;
}

/* Not even worth warning about this */
__attribute__((weak))
void sync(void) ;
void sync(void) {
}

/* Error ignore */

extern int __fgetc_unlocked(FILE *f);
extern int __fputc_unlocked(int c, FILE *f);

__attribute__((weak))
int _IO_getc(FILE *f) ;
int _IO_getc(FILE *f) {
  return __fgetc_unlocked(f);
}

__attribute__((weak))
int _IO_putc(int c, FILE *f) ;
int _IO_putc(int c, FILE *f) {
  return __fputc_unlocked(c, f);
}

__attribute__((weak))
int mkdir(const char *pathname, mode_t mode) ;
int mkdir(const char *pathname, mode_t mode) {
  klee_warning("ignoring (EIO)");
  errno = EIO;
  return -1;
}

/*int mkfifo(const char *pathname, mode_t mode) __attribute__((weak));
int mkfifo(const char *pathname, mode_t mode) {
  klee_warning("ignoring (EIO)");
  errno = EIO;
  return -1;
}*/

__attribute__((weak))
int mknod(const char *pathname, mode_t mode, dev_t dev) ;
int mknod(const char *pathname, mode_t mode, dev_t dev) {
  klee_warning("ignoring (EIO)");
  errno = EIO;
  return -1;
}

__attribute__((weak))
int link(const char *oldpath, const char *newpath) ;
int link(const char *oldpath, const char *newpath) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;  
}

__attribute__((weak))
int symlink(const char *oldpath, const char *newpath) ;
int symlink(const char *oldpath, const char *newpath) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;  
}

__attribute__((weak))
int rename(const char *oldpath, const char *newpath) ;
int rename(const char *oldpath, const char *newpath) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;  
}

__attribute__((weak))
int nanosleep(const struct timespec *req, struct timespec *rem) ;
int nanosleep(const struct timespec *req, struct timespec *rem) {
  klee_thread_sleep(req->tv_sec);
  return 0;
}

/* XXX why can't I call this internally? */
__attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *res) ;
int clock_gettime(clockid_t clk_id, struct timespec *res) {
  /* Fake */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  res->tv_sec = tv.tv_sec;
  res->tv_nsec = tv.tv_usec * 1000;
  return 0;
}

__attribute__((weak))
int clock_settime(clockid_t clk_id, const struct timespec *res) ;
int clock_settime(clockid_t clk_id, const struct timespec *res) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

time_t time(time_t *t) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (t)
    *t = tv.tv_sec;
  return tv.tv_sec;
}

clock_t times(struct tms *buf) {
  /* Fake */
  buf->tms_utime = 0;
  buf->tms_stime = 0;
  buf->tms_cutime = 0;
  buf->tms_cstime = 0;
  return 0;
}

__attribute__((weak))
struct utmpx *getutxent(void) ;
struct utmpx *getutxent(void) {
  return (struct utmpx*) getutent();
}

__attribute__((weak))
void setutxent(void) ;
void setutxent(void) {
  setutent();
}

__attribute__((weak))
void endutxent(void) ;
void endutxent(void) {
  endutent();
}

__attribute__((weak))
int utmpxname(const char *file) ;
int utmpxname(const char *file) {
  utmpname(file);
  return 0;
}

__attribute__((weak))
int euidaccess(const char *pathname, int mode) ;
int euidaccess(const char *pathname, int mode) {
  return access(pathname, mode);
}

__attribute__((weak))
int eaccess(const char *pathname, int mode) ;
int eaccess(const char *pathname, int mode) {
  return euidaccess(pathname, mode);
}

__attribute__((weak))
int group_member (gid_t __gid) ;
int group_member (gid_t __gid) {
  return ((__gid == getgid ()) || (__gid == getegid ()));
}

__attribute__((weak))
int utime(const char *filename, const struct utimbuf *buf) ;
int utime(const char *filename, const struct utimbuf *buf) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int utimes(const char *filename, const struct timeval times[2]) ;
int utimes(const char *filename, const struct timeval times[2]) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int futimes(int fd, const struct timeval times[2]) ;
int futimes(int fd, const struct timeval times[2]) {
  klee_warning("ignoring (EBADF)");
  errno = EBADF;
  return -1;
}

int strverscmp (__const char *__s1, __const char *__s2) {
  return strcmp(__s1, __s2); /* XXX no doubt this is bad */
}

__attribute__((weak))
unsigned int gnu_dev_major(unsigned long long int __dev);
unsigned int gnu_dev_major(unsigned long long int __dev) {
  return ((__dev >> 8) & 0xfff) | ((unsigned int) (__dev >> 32) & ~0xfff);
}

__attribute__((weak))
unsigned int gnu_dev_minor(unsigned long long int __dev);
unsigned int gnu_dev_minor(unsigned long long int __dev) {
  return (__dev & 0xff) | ((unsigned int) (__dev >> 12) & ~0xff);
}

__attribute__((weak))
unsigned long long int gnu_dev_makedev(unsigned int __major, unsigned int __minor);
unsigned long long int gnu_dev_makedev(unsigned int __major, unsigned int __minor) {
  return ((__minor & 0xff) | ((__major & 0xfff) << 8)
	  | (((unsigned long long int) (__minor & ~0xff)) << 12)
	  | (((unsigned long long int) (__major & ~0xfff)) << 32));
}

__attribute__((weak))
char *canonicalize_file_name (const char *name);
char *canonicalize_file_name (const char *name) {
  char *res = malloc(PATH_MAX);
  char *rp_res = realpath(name, res);
  if (!rp_res)
    free(res);
  return rp_res;
}

__attribute__((weak))
int getloadavg(double loadavg[], int nelem);
int getloadavg(double loadavg[], int nelem) {
  klee_warning("ignoring (-1 result)");
  return -1;
}

/* ACL */

/* FIXME: We need autoconf magic for this. */

#ifdef HAVE_SYS_ACL_H

#include <sys/acl.h>

__attribute__((weak))
int acl_delete_def_file(const char *path_p) ;
int acl_delete_def_file(const char *path_p) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int acl_extended_file(const char path_p) ;
int acl_extended_file(const char path_p) {
  klee_warning("ignoring (ENOENT)");
  errno = ENOENT;
  return -1;
}

__attribute__((weak))
int acl_entries(acl_t acl) ;
int acl_entries(acl_t acl) {
  klee_warning("ignoring (EINVAL)");
  errno = EINVAL;
  return -1;
}

__attribute__((weak))
acl_t acl_from_mode(mode_t mode) ;
acl_t acl_from_mode(mode_t mode) {
  klee_warning("ignoring (ENOMEM)");
  errno = ENOMEM;
  return NULL;
}

__attribute__((weak))
acl_t acl_get_fd(int fd) ;
acl_t acl_get_fd(int fd) {
  klee_warning("ignoring (ENOMEM)");
  errno = ENOMEM;
  return NULL;
}

__attribute__((weak))
acl_t acl_get_file(const char *pathname, acl_type_t type) ;
acl_t acl_get_file(const char *pathname, acl_type_t type) {
  klee_warning("ignoring (ENONMEM)");
  errno = ENOMEM;
  return NULL;
}

__attribute__((weak))
int acl_set_fd(int fd, acl_t acl) ;
int acl_set_fd(int fd, acl_t acl) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int acl_set_file(const char *path_p, acl_type_t type, acl_t acl) ;
int acl_set_file(const char *path_p, acl_type_t type, acl_t acl) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int acl_free(void *obj_p) ;
int acl_free(void *obj_p) {
  klee_warning("ignoring (EINVAL)");
  errno = EINVAL;
  return -1;
}

#endif

__attribute__((weak))
int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) ;
int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int umount(const char *target) ;
int umount(const char *target) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int umount2(const char *target, int flags) ;
int umount2(const char *target, int flags) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int swapon(const char *path, int swapflags) ;
int swapon(const char *path, int swapflags) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int swapoff(const char *path) ;
int swapoff(const char *path) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setgid(gid_t gid);
int setgid(gid_t gid) {
  klee_warning("silently ignoring (returning 0)");
  return 0;
}

__attribute__((weak))
int setgroups(size_t size, const gid_t *list) ;
int setgroups(size_t size, const gid_t *list) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int sethostname(const char *name, size_t len) ;
int sethostname(const char *name, size_t len) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setpgid(pid_t pid, pid_t pgid) ;
int setpgid(pid_t pid, pid_t pgid) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setpgrp(void) ;
int setpgrp(void) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setpriority(__priority_which_t which, id_t who, int prio) ;
int setpriority(__priority_which_t which, id_t who, int prio) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setresgid(gid_t rgid, gid_t egid, gid_t sgid) ;
int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setresuid(uid_t ruid, uid_t euid, uid_t suid) ;
int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim) ;
int setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim) {
#if 0
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
#endif
  return 0;
}

__attribute__((weak))
int setrlimit64(__rlimit_resource_t resource, const struct rlimit64 *rlim) ;
int setrlimit64(__rlimit_resource_t resource, const struct rlimit64 *rlim) {
#if 0
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
#endif
  return 0;
}

__attribute__((weak))
pid_t setsid(void) ;
pid_t setsid(void) {
  klee_warning("ignoring (EPERM)");
  return 0;
}

__attribute__((weak))
int settimeofday(const struct timeval *tv, const struct timezone *tz) ;
int settimeofday(const struct timeval *tv, const struct timezone *tz) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int setuid(uid_t uid) ;
int setuid(uid_t uid) {
  klee_warning("silently ignoring (returning 0)");
  return 0;
}

__attribute__((weak))
int reboot(int flag) ;
int reboot(int flag) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int mlock(const void *addr, size_t len) ;
int mlock(const void *addr, size_t len) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
int munlock(const void *addr, size_t len) ;
int munlock(const void *addr, size_t len) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

int pause(void) __attribute__((weak));
int pause(void) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

__attribute__((weak))
ssize_t readahead(int fd, off64_t *offset, size_t count) ;
ssize_t readahead(int fd, off64_t *offset, size_t count) {
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

void openlog(const char *ident, int option, int facility) {
  klee_warning("ignoring");
}

void syslog(int priority, const char *format, ...) {
  klee_warning("ignoring");
}

void closelog(void) {
  klee_warning("ignoring");
}

void vsyslog(int priority, const char *format, va_list ap) {
  klee_warning("ignoring");
}
