#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([uClibc++], [0.4.0], [klover-support@us.fujitsu.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects no-define nostdinc])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O
AM_PROG_AR

AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_CFLAGS])
AC_ARG_WITH([llvm-build-dir],
  [AS_HELP_STRING([--with-llvm-build-dir],
    [location of LLVM build directory])],
  [],
  [AC_MSG_FAILURE([Missing --with-llvm-build-dir=<llvm-build-location>])])
with_llvm_bin="$with_llvm_build_dir/bin"

AC_ARG_WITH([klee-src-dir],
  [AS_HELP_STRING([--with-klee-src-dir],
    [location of KLEE build directory])],
  [],
  [AC_MSG_FAILURE([Missing --with-klee-src-dir=<klee-src-location>])])

AC_SUBST(KLEE_INCLUDES,"-I$with_klee_src_dir/include")

AM_CXXFLAGS="-emit-llvm -I\${top_srcdir}/include $AM_CXXFLAGS"
AM_CFLAGS="-emit-llvm -I\${top_srcdir}/include $AM_CFLAGS"

AC_SUBST(CC,$with_llvm_bin/clang)
AC_SUBST(CXX,$with_llvm_bin/clang++)
AC_SUBST(AR,$with_llvm_bin/llvm-link)
AC_SUBST(ARFLAGS,-o)
AC_SUBST(RANLIB,echo)

llvm_libs="`$with_llvm_bin/llvm-config --libs jit bitreader bitwriter ipo linker engine`"
AC_SUBST(LLVM_LIBS,$llvm_libs)
llvm_ldflags="`$with_llvm_bin/llvm-config --ldflags`"
AC_SUBST(LLVM_LDFLAGS,$llvm_ldflags)

AC_CONFIG_FILES([Makefile
                 POSIX/Makefile
                 llvm-ex/Makefile
                 libsupc++/Makefile
                 src/Makefile ])
AC_OUTPUT
