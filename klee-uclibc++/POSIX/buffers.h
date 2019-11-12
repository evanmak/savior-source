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

#ifndef BUFFERS_H_
#define BUFFERS_H_

#include <unistd.h>
#include <sys/uio.h>

#include "common.h"
#include "multiprocess.h"

////////////////////////////////////////////////////////////////////////////////
// Event Queue Utility
////////////////////////////////////////////////////////////////////////////////

#define EVENT_READ  (1 << 0)
#define EVENT_WRITE (1 << 1)

typedef struct {
  wlist_id_t *queue;
  unsigned int count;
} event_queue_t;

void _event_queue_init(event_queue_t *q, unsigned count, int shared);
void _event_queue_finalize(event_queue_t *q);

int _event_queue_register(event_queue_t *q, wlist_id_t wlist);
int _event_queue_clear(event_queue_t *q, wlist_id_t wlist);

void _event_queue_notify(event_queue_t *q);

////////////////////////////////////////////////////////////////////////////////
// Stream Buffers
////////////////////////////////////////////////////////////////////////////////

#define BUFFER_STATUS_DESTROYING        (1<<0)
#define BUFFER_STATUS_CLOSED            (1<<1)
#define BUFFER_STATUS_SYM_READS         (1<<2)

// A basic producer-consumer data structure
typedef struct {
  char *contents;
  size_t max_size;
  size_t max_rsize;

  size_t start;
  size_t size;

  event_queue_t evt_queue;
  wlist_id_t empty_wlist;
  wlist_id_t full_wlist;

  unsigned int queued;
  unsigned short status;
} stream_buffer_t;

stream_buffer_t *_stream_create(size_t max_size, int shared);
void _stream_destroy(stream_buffer_t *buff);

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count);
ssize_t _stream_write(stream_buffer_t *buff, const char *src, size_t count);

ssize_t _stream_readv(stream_buffer_t *buff, const struct iovec *iov, int iovcnt);
ssize_t _stream_writev(stream_buffer_t *buff, const struct iovec *iov, int iovcnt);

void _stream_close(stream_buffer_t *buff);

int _stream_register_event(stream_buffer_t *buff, wlist_id_t wlist);
int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist);

static inline int _stream_is_empty(stream_buffer_t *buff) {
  return (buff->size == 0);
}

static inline int _stream_is_full(stream_buffer_t *buff) {
  return (buff->size == buff->max_size);
}

static inline int _stream_is_closed(stream_buffer_t *buff) {
  return (buff->status & BUFFER_STATUS_CLOSED);
}

static inline void _stream_set_rsize(stream_buffer_t *buff, size_t rsize) {
  if (rsize == 0)
    rsize = 1;

  if (rsize > buff->max_size)
    rsize = buff->max_size;

  buff->max_rsize = rsize;
}

////////////////////////////////////////////////////////////////////////////////
// Block Buffers
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  char *contents;
  size_t max_size;
  size_t size;
} block_buffer_t;


void _block_init(block_buffer_t *buff, size_t max_size);
void _block_finalize(block_buffer_t *buff);

ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, size_t offset);
ssize_t _block_write(block_buffer_t *buff, const char *src, size_t count, size_t offset);

ssize_t _block_readv(block_buffer_t *buff, const struct iovec *iov, int iovcnt,
    size_t count, size_t offset);
ssize_t _block_writev(block_buffer_t *buff, const struct iovec *iov, int iovcnt,
    size_t count, size_t offset);


#endif /* BUFFERS_H_ */
