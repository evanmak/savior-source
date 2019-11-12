inst_LIBRARIES = libposix.a
instdir = $(top_srcdir)/lib

libposix_a_SOURCES = buffers.c\
                     common.c\
                     fd.c\
                     fd_init.c\
                     files.c\
                     illegal.c\
                     klee_init_env.c\
                     misc.c\
                     models.c\
                     multiprocess_init.c\
                     netlink.c\
                     pipes.c\
                     processes.c\
                     procsync.c\
                     signals.c\
                     sockets.c\
                     stubs.c\
                     threads.c\
                     threadsync.c

AM_CFLAGS = @AM_CFLAGS@ @KLEE_INCLUDES@ -D_GNU_SOURCE -D__STDC_FORMAT_MACROS -fomit-frame-pointer -fPIC -std=gnu89 -Wall -W -Wno-unused-parameter -Wwrite-strings 
