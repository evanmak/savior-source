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

#ifndef POSIX_CONFIG_H_
#define POSIX_CONFIG_H_

////////////////////////////////////////////////////////////////////////////////
// System Limits
////////////////////////////////////////////////////////////////////////////////

#define MAX_THREADS         16
#define MAX_PROCESSES       8

#define MAX_SEMAPHORES      8

#define MAX_EVENTS          4

#define MAX_FDS             64
#define MAX_FILES           16

#define MAX_PATH_LEN        75

#define MAX_PORTS           32
#define MAX_UNIX_EPOINTS    32
#define MAX_NETLINK_EPOINTS 32

#define MAX_PENDING_CONN    4

#define MAX_MMAPS           32

#define MAX_STDINSIZE       16

#define MAX_DGRAM_SIZE          4096
#define MAX_NUMBER_DGRAMS       8
#define STREAM_BUFFER_SIZE      4096
#define PIPE_BUFFER_SIZE        4096
#define SENDFILE_BUFFER_SIZE    256

////////////////////////////////////////////////////////////////////////////////
// Enabled Components
////////////////////////////////////////////////////////////////////////////////

#define HAVE_FAULT_INJECTION    1
//#define HAVE_SYMBOLIC_CTYPE     1
//#define HAVE_POSIX_SIGNALS	1


#endif /* POSIX_CONFIG_H_ */
