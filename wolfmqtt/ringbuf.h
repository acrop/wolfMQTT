
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

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>
#include <stdint.h>

#if !defined(_MSC_VER) || _MSC_VER > 1600
#include <stdbool.h>
#else
#include "win32/msvc_stdbool.h"
#endif

#if !defined(_MSC_VER)
#include <stdatomic.h>
#else
#include "win32/msvc_stdatomic.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ringbuf {
	_Atomic(size_t) read_idx;
	_Atomic(size_t) write_idx;
	size_t capacity;
	size_t capacity_mask;
	uint8_t *data;
};

size_t
ringbuf_power_of_2(size_t expect_capacity);

/* From https://github.com/szanni/ringbuf/commits/master */
/*!
 * \brief Allocate a new SPSC ring buffer.
 *
 * Allocate a new single producer, single consumer ring buffer.
 * The capacity will be rounded up to the next power of 2.
 *
 * \param capacity Capacity of the ring buffer.
 * \return A pointer to a newly allocated ring buffer, NULL on error.
 */
bool
ringbuf_init(struct ringbuf *rb, uint8_t *data, size_t capacity);

/*!
 * \brief Reset the ring buffer to the initial state.
 *
 * \param rb Ring buffer instance.
 * \return void
 */
void
ringbuf_reset(struct ringbuf *rb);

/*!
 * \brief Query available space to writing buffer
 * \warning Only call this function from a single producer thread.
 *
 * \param rb Ring buffer instance.
 * \return Number of bytes available for writing.
 */
size_t
ringbuf_write_available(struct ringbuf *rb);

/*!
 * \brief Write to ring buffer.
 * \warning Only call this function from a single producer thread.
 *
 * \param rb Ring buffer instance.
 * \param buf Buffer holding data to be written to ring buffer.
 * \param buf_size Buffer size in bytes.
 * \return Number of bytes written to ring buffer.
 */
size_t
ringbuf_write(struct ringbuf *rb, const uint8_t *buf, size_t buf_size);

/*!
 * \brief Query available arrived data size for reading.
 * \warning Only call this function from a single consumer thread.
 *
 * \param rb Ring buffer instance.
 * \return Number of bytes available for reading.
 */
size_t
ringbuf_read_available(struct ringbuf *rb);

/*!
 * \brief Read from ring buffer.
 * \warning Only call this function from a single consumer thread.
 *
 * \param rb Ring buffer instance.
 * \param buf Buffer to copy data to from ring buffer.
 * \param buf_size Buffer size in bytes.
 * \param consume If consume, then when the data are readed, the readed
 *                data will be consumed from the ring buffer; otherwise
 *                the readed data will preserve in the ring buffer for
 *                latter reading.
 * \return Number of bytes read from ring buffer.
 */
size_t
ringbuf_read(struct ringbuf *rb, uint8_t *buf, size_t buf_size, bool consume);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
