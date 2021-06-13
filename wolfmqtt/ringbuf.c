
/*
 * Copyright (c) 2018, Angelo Haller
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "ringbuf.h"

bool
ringbuf_init(struct ringbuf *rb, uint8_t *data, size_t capacity)
{
	size_t power_of_two;

	for (power_of_two = 1; 1ull << power_of_two < capacity; ++power_of_two) {}

  if (capacity != 1 << power_of_two)
  {
    return false;
  }
  rb->data = data;

	rb->capacity = capacity;
	rb->capacity_mask = rb->capacity - 1;
	ringbuf_reset(rb);
	return true;
}

void
ringbuf_reset(struct ringbuf *rb)
{
	atomic_store_explicit(&rb->read_idx, 0, memory_order_seq_cst);
	atomic_store_explicit(&rb->write_idx, rb->capacity * 2, memory_order_seq_cst);
}

static inline size_t
_ringbuf_size(struct ringbuf *rb, size_t read_idx, size_t write_idx)
{
	return (write_idx - read_idx) & ((rb->capacity_mask << 1) | 1);
}

static inline size_t
_ringbuf_min(size_t a, size_t b)
{
	if (a < b)
		return a;
	else
		return b;
}

//! True ring buffer array index.
#define _RINGBUF_IDX(idx) ((idx) & rb->capacity_mask)

size_t
ringbuf_write_available(struct ringbuf *rb)
{
	size_t write_idx = atomic_load_explicit(&rb->write_idx, memory_order_relaxed);
	size_t read_idx = atomic_load_explicit(&rb->read_idx, memory_order_acquire);
	size_t size = _ringbuf_size(rb, read_idx, write_idx);
	return rb->capacity - size;
}

size_t
ringbuf_write(struct ringbuf *rb, const uint8_t *buf, size_t buf_size)
{
	size_t write_idx = atomic_load_explicit(&rb->write_idx, memory_order_relaxed);
	size_t read_idx = atomic_load_explicit(&rb->read_idx, memory_order_acquire);
	size_t size = _ringbuf_size(rb, read_idx, write_idx);

	if (size < rb->capacity) {
		size_t write0, write1;
		size_t write = _ringbuf_min(rb->capacity - size, buf_size);
		size_t write_overflow = _RINGBUF_IDX(write_idx) + write;
		if (write_overflow > rb->capacity) {
			write1 = write_overflow & rb->capacity_mask;
			write0 = write - write1;
		}
		else {
			write0 = write;
			write1 = 0;
		}

		memcpy(&rb->data[_RINGBUF_IDX(write_idx)], buf, write0);

		if (write1)
			memcpy(rb->data, buf + write0, write1);

		write_idx = (write_idx + write < rb->capacity * 4) ? write_idx + write : rb->capacity * 2 + write1;
		atomic_store_explicit(&rb->write_idx, write_idx, memory_order_release);
		return write;
	}
	errno = EWOULDBLOCK;
	return 0;
}

size_t
ringbuf_read_available(struct ringbuf *rb)
{
	size_t read_idx = atomic_load_explicit(&rb->read_idx, memory_order_relaxed);
	size_t write_idx = atomic_load_explicit(&rb->write_idx, memory_order_acquire);
	size_t size = _ringbuf_size(rb, read_idx, write_idx);
	return size;
}

size_t
ringbuf_read(struct ringbuf *rb, uint8_t *buf, size_t buf_size, bool consume)
{
	size_t read_idx = atomic_load_explicit(&rb->read_idx, memory_order_relaxed);
	size_t read_idx_saved = read_idx;
	size_t write_idx = atomic_load_explicit(&rb->write_idx, memory_order_acquire);
	size_t size = _ringbuf_size(rb, read_idx, write_idx);

	if (size > 0)
	{
		size_t read0, read1;
		size_t read = _ringbuf_min(size, buf_size);
		size_t read_overflow = _RINGBUF_IDX(read_idx) + read;
		if (read_overflow > rb->capacity) {
			read1 = read_overflow & rb->capacity_mask;
			read0 = read - read1;
		}
		else {
			read0 = read;
			read1 = 0;
		}

		if (buf) {
			memcpy(buf, &rb->data[_RINGBUF_IDX(read_idx)], read0);
		}

		if (read1 && buf)
			memcpy(buf + read0, rb->data, read1);

		read_idx = (read_idx + read < rb->capacity * 2) ? read_idx + read : read1;
		if (consume) {
			if (!atomic_compare_exchange_strong(&rb->read_idx, &read_idx_saved, read_idx)) {
				errno = EINVAL;
				return 0;
			}
		}
		return read;
	}
	errno = EWOULDBLOCK;
	return 0;
}
