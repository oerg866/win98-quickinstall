#ifndef RINGBUF_H
#define RINGBUF_H

/*
 * RingBuffer implementation for LUNMERCY
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint8_t *buf;
    size_t size;
    size_t r;
	size_t w;
    size_t avail;
    pthread_mutex_t lock;
} ringbuf;

ringbuf *rb_init(size_t size);

size_t rb_avail(ringbuf *rb);
size_t rb_space(ringbuf *rb);

bool rb_read(ringbuf *rb, uint8_t *dst, size_t len);            /* Read data from the ring buffer into another buffer */
bool rb_write(ringbuf *rb, uint8_t *src, size_t len);           /* Write data to the ring buffer from another buffer */
void rb_destroy(ringbuf *rb);                                   /* Destroy and free the ring buffer */
void rb_reset(ringbuf *rb);                                     /* Reset the ring buffer */
bool rb_read_fwrite(ringbuf *rb, size_t len, FILE *file);       /* Write data from ringbuffer directly into a file */
bool rb_fread_write(ringbuf *rb, size_t len, FILE *file);       /* Read data from a file directly into a ringbuffer*/

#define rb_getUInt32(rb, dst) (rb_read(rb, (uint8_t*) dst, 4))  /* Read a 32-bit unsigned integer value from the ring buffer into the pointer dst */
#define rb_getUInt16(rb, dst) (rb_read(rb, (uint8_t*) dst, 2))  /* Read a 16-bit unsigned integer value into the ring buffer pointer dst */
#define rb_getUInt8(rb, dst) (rb_read(rb, (uint8_t*) dst, 1))   /* Read a 8-bit unsigned integer value into the ring buffer pointer dst */

#endif