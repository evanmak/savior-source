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

#include "buffers.h"

#include "common.h"
#include "signals.h"

#include <stdlib.h>
#include <stdio.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Event Queue Utility
////////////////////////////////////////////////////////////////////////////////

void _event_queue_init(event_queue_t *q, unsigned count, int shared) {
  q->queue = (wlist_id_t*)malloc(count * sizeof(wlist_id_t));

  memset(q->queue, 0, count * sizeof(wlist_id_t));

  if (shared)
    klee_make_shared(q->queue, count * sizeof(wlist_id_t));

  q->count = count;
}

void _event_queue_finalize(event_queue_t *q) {
  free(q->queue);
}

int _event_queue_register(event_queue_t *q, wlist_id_t wlist) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] == 0) {
      q->queue[i] = wlist;
      return 0;
    }
  }

  return -1;
}

int _event_queue_clear(event_queue_t *q, wlist_id_t wlist) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] == wlist) {
      q->queue[i] = 0;
      return 0;
    }
  }

  return -1;
}

void _event_queue_notify(event_queue_t *q) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] > 0)
      __thread_notify_all(q->queue[i]);
  }

  memset(q->queue, 0, q->count*sizeof(wlist_id_t));
}

////////////////////////////////////////////////////////////////////////////////
// Stream Buffers
////////////////////////////////////////////////////////////////////////////////

void __notify_event(stream_buffer_t *buff, char event) {
  if (event & EVENT_READ)
    __thread_notify_all(buff->empty_wlist);

  if (event & EVENT_WRITE)
    __thread_notify_all(buff->full_wlist);

  _event_queue_notify(&buff->evt_queue);
}

stream_buffer_t *_stream_create(size_t max_size, int shared) {
  stream_buffer_t *buff = (stream_buffer_t*)malloc(sizeof(stream_buffer_t));

  memset(buff, 0, sizeof(stream_buffer_t));
  buff->contents = (char*) malloc(max_size);
  buff->max_size = max_size;
  buff->max_rsize = max_size;
  buff->queued = 0;
  buff->empty_wlist = klee_get_wlist();
  buff->full_wlist = klee_get_wlist();
  _event_queue_init(&buff->evt_queue, MAX_EVENTS, shared);

  if (shared) {
    klee_make_shared(buff, sizeof(stream_buffer_t));
    klee_make_shared(buff->contents, max_size);
  }

  return buff;
}

void _stream_destroy(stream_buffer_t *buff) {
  __notify_event(buff, EVENT_READ | EVENT_WRITE);

  free(buff->contents);
  _event_queue_finalize(&buff->evt_queue);

  if (buff->queued == 0) {
    free(buff);
  } else {
    buff->status |= BUFFER_STATUS_DESTROYING;
  }
}

void _stream_close(stream_buffer_t *buff) {
  __notify_event(buff, EVENT_READ | EVENT_WRITE);

  buff->status |= BUFFER_STATUS_CLOSED;
}

ssize_t _stream_readv(stream_buffer_t *buff, const struct iovec *iov, int iovcnt) {
  size_t count = __count_iovec(iov, iovcnt);

  if (count == 0) {
    return 0;
  }

  while (buff->size == 0) {
    if (buff->status & BUFFER_STATUS_CLOSED) {
      return 0;
    }

    buff->queued++;
    __thread_sleep(buff->empty_wlist);
    buff->queued--;

    if (buff->status & BUFFER_STATUS_DESTROYING) {
      if (buff->queued == 0)
        free(buff);

      return -1;
    }
  }

  if (buff->size < count)
    count = buff->size;
  if (buff->max_rsize < count)
    count = buff->max_rsize;

  if (buff->status & BUFFER_STATUS_SYM_READS) {
    count = __fork_values(1, count, __KLEE_FORK_DEFAULT);
    klee_event(__KLEE_EVENT_PKT_FRAGMENT, __concretize_size(count));
  }

  int i;
  size_t offset = buff->start;
  size_t remaining = count;

  for (i = 0; i < iovcnt; i++) {
    if (remaining == 0)
      break;

    size_t cur_count = (remaining < iov[i].iov_len) ? remaining : iov[i].iov_len;

    if (offset + cur_count > buff->max_size) {
      size_t overflow = (offset + cur_count) % buff->max_size;

      memcpy(iov[i].iov_base, &buff->contents[offset], cur_count - overflow);
      memcpy(&((char*)iov[i].iov_base)[cur_count-overflow], &buff->contents[0], overflow);
      offset = overflow;
    } else {
      memcpy(iov[i].iov_base, &buff->contents[offset], cur_count);
      offset += cur_count;
    }
    remaining -= cur_count;
  }

  buff->start = offset;
  buff->size -= count;

  __notify_event(buff, EVENT_WRITE);

  return count;
}

ssize_t _stream_writev(stream_buffer_t *buff, const struct iovec *iov, int iovcnt) {
  size_t count = __count_iovec(iov, iovcnt);

  if (count == 0) {
    return 0;
  }

  if (buff->status & BUFFER_STATUS_CLOSED) {
    return 0;
  }

  while (buff->size == buff->max_size) {
    buff->queued++;
    __thread_sleep(buff->full_wlist);
    buff->queued--;

    if (buff->status & BUFFER_STATUS_DESTROYING) {
      if (buff->queued == 0)
        free(buff);

      return -1;
    }

    if (buff->status & BUFFER_STATUS_CLOSED) {
      return 0;
    }
  }

  if (count > buff->max_size - buff->size)
    count = buff->max_size - buff->size;

  int i;
  size_t offset = (buff->start + buff->size) % buff->max_size;
  size_t remaining = count;

  for (i = 0; i < iovcnt; i++) {
    if (remaining == 0)
      break;

    size_t cur_count = (remaining < iov[i].iov_len) ? remaining : iov[i].iov_len;

    if (offset + cur_count > buff->max_size) {
      size_t overflow = (offset + cur_count) % buff->max_size;

      memcpy(&buff->contents[offset], iov[i].iov_base, cur_count - overflow);
      memcpy(&buff->contents[0], &((char*)iov[i].iov_base)[cur_count-overflow], overflow);
      offset = overflow;
    } else {
      memcpy(&buff->contents[offset], iov[i].iov_base, cur_count);
      offset += cur_count;
    }
    remaining -= cur_count;
  }

  buff->size += count;

  __notify_event(buff, EVENT_READ);

  return count;
}

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count) {
  struct iovec iov = { .iov_base = dest, .iov_len = count };

  return _stream_readv(buff, &iov, 1);
}

ssize_t _stream_write(stream_buffer_t *buff, const char *src, size_t count) {
  struct iovec iov = { .iov_base = (char*)src, .iov_len = count };

  return _stream_writev(buff, &iov, 1);
}

int _stream_register_event(stream_buffer_t *buff, wlist_id_t wlist) {
  return _event_queue_register(&buff->evt_queue, wlist);
}

int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist) {
  return _event_queue_clear(&buff->evt_queue, wlist);
}

////////////////////////////////////////////////////////////////////////////////
// Block Buffers
////////////////////////////////////////////////////////////////////////////////

void _block_init(block_buffer_t *buff, size_t max_size) {
  memset(buff, 0, sizeof(block_buffer_t));
  buff->contents = (char*)malloc(max_size);
  buff->max_size = max_size;
  buff->size = 0;
}

void _block_finalize(block_buffer_t *buff) {
  free(buff->contents);
}

ssize_t _block_readv(block_buffer_t *buff, const struct iovec *iov, int iovcnt,
    size_t count, size_t offset) {
  if (offset > buff->size)
    return -1;

  if (!count)
    count = __count_iovec(iov, iovcnt);

  if (offset + count > buff->size)
    count = buff->size - offset;

  if (count == 0)
    return 0;

  int i;
  size_t remaining = count;
  for (i = 0; i < iovcnt; i++) {
    if (remaining == 0)
      break;

    size_t cur_count = (remaining < iov[i].iov_len) ? remaining : iov[i].iov_len;
    memcpy(iov[i].iov_base, &buff->contents[offset+(count-remaining)], cur_count);
    remaining -= cur_count;
  }

  return count;
}

ssize_t _block_writev(block_buffer_t *buff, const struct iovec *iov, int iovcnt,
    size_t count, size_t offset) {
  if (offset > buff->max_size)
    return -1;

  if (!count)
    count = __count_iovec(iov, iovcnt);

  if (offset + count > buff->max_size)
    count = buff->max_size - offset;

  if (count == 0)
    return 0;

  int i;
  size_t remaining = count;
  for (i = 0; i < iovcnt; i++) {
    if (remaining == 0)
      break;

    size_t cur_count = (remaining < iov[i].iov_len) ? remaining : iov[i].iov_len;
    memcpy(&buff->contents[offset+(count-remaining)], iov[i].iov_base, cur_count);
    remaining -= cur_count;
  }

  buff->size = offset + count;

  return count;
}

ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, size_t offset) {
  struct iovec iov = { .iov_base = dest, .iov_len = count };
  return _block_readv(buff, &iov, 1, 0, offset);
}

ssize_t _block_write(block_buffer_t *buff, const char *src, size_t count, size_t offset) {
  struct iovec iov = { .iov_base = (char*)src, .iov_len = count };
  return _block_writev(buff, &iov, 1, 0, offset);
}
