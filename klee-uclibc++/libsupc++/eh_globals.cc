// -*- C++ -*- Manage the thread-local exception globals.
// Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006
// Free Software Foundation, Inc.
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING.  If not, write to
// the Free Software Foundation, 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

// #include <bits/c++config.h>
#include <exception>
#include <cstdlib>
#include "cxxabi.h"
#include "unwind-cxx.h"
// #include "bits/gthr.h"

// In a freestanding environment, these functions may not be
// available -- but for now, we assume that they are.
extern "C" void *malloc (std::size_t) throw ();
extern "C" void free(void *) throw ();

using namespace __cxxabiv1;

#if _GLIBCXX_HAVE_TLS

namespace __gnu_internal
{
  using namespace abi;
  using namespace std;

  __cxa_eh_globals*
  get_global() throw()
  {
    static __thread __cxa_eh_globals global;
    return &global;
  }
}

extern "C" __cxa_eh_globals*
__cxxabiv1::__cxa_get_globals_fast() throw()
{ return __gnu_internal::get_global(); }

extern "C" __cxa_eh_globals*
__cxxabiv1::__cxa_get_globals() throw()
{ return __gnu_internal::get_global(); }


#else

// Single-threaded fallback buffer.
static __cxa_eh_globals eh_globals;

extern "C" __cxa_eh_globals*
__cxxabiv1::__cxa_get_globals_fast() throw()
{ return &eh_globals; }

extern "C" __cxa_eh_globals*
__cxxabiv1::__cxa_get_globals() throw()
{ return &eh_globals; }

#endif
