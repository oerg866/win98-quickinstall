/*
 *  RingBuffer implementation for LUNMERCY
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include "ringbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>


static inline void rb_lock(ringbuf *rb) {
    pthread_mutex_lock(&rb->lock);
}

static inline void rb_unlock(ringbuf *rb) {
    pthread_mutex_unlock(&rb->lock);
}

inline size_t rb_avail(ringbuf *rb) {
    size_t ret;
    rb_lock(rb);
    ret = rb->avail;
    rb_unlock(rb);
    return ret;
}

inline size_t rb_space(ringbuf *rb) {
    size_t ret;
    rb_lock(rb);
    ret = rb->size - rb->avail;
    rb_unlock(rb);
    return ret;
}

ringbuf *rb_init(size_t size) {
    ringbuf *ret =  calloc(sizeof(ringbuf), 1);
    assert(ret);

    if (!ret) return NULL;

    ret->buf = malloc(size);

    if (!ret->buf) { rb_destroy(ret); return NULL; }
    
    ret->size = size;
    
    pthread_mutex_init(&ret->lock, NULL);
    
    return ret;
}

bool rb_read(ringbuf *rb, uint8_t *dst, size_t len) {
    assert(len <= rb->size);

    while(rb_avail(rb) < len) {
        sched_yield();
    }

    size_t readSize1 = MIN(rb->size - rb->r, len);
    size_t readSize2 = len - readSize1;

    memcpy(dst, &rb->buf[rb->r], readSize1);
    if (readSize2) memcpy(dst + readSize1, rb->buf, readSize2);

    rb_lock(rb);
    rb->r = readSize2 ? readSize2 : rb->r + readSize1; // No need to do modulo here, just check if we wrapped around and use that value instead.
    rb->avail -= len;
    rb_unlock(rb);

    return true;
}

bool rb_write(ringbuf *rb, uint8_t *src, size_t len) {
    assert(len <= rb->size);

    while(rb_space(rb) < len) {
        sched_yield();
    }


    size_t writeSize1 = MIN(rb->size - rb->w, len);
    size_t writeSize2 = len - writeSize1;

    memcpy(&rb->buf[rb->w], src, writeSize1);
    if (writeSize2) memcpy(rb->buf, src + writeSize1, writeSize2);

    rb_lock(rb);
    rb->w = writeSize2 ? writeSize2 : rb->w + writeSize1; // No need to do modulo here, just check if we wrapped around and use that value instead.
    rb->avail += len;
    rb_unlock(rb);

    return true;
}

bool rb_read_fwrite(ringbuf *rb, size_t len, FILE *file) {
    assert(len <= rb->size);

    while(rb_avail(rb) < len) {
        sched_yield();
    }

    size_t readSize1 = MIN(rb->size - rb->r, len);
    size_t readSize2 = len - readSize1;

    size_t written = fwrite_unlocked(&rb->buf[rb->r], 1, readSize1, file);
    if (readSize2) written += fwrite_unlocked(rb->buf, 1, readSize2, file);

    rb_lock(rb);
    rb->r = readSize2 ? readSize2 : rb->r + readSize1; // No need to do modulo here, just check if we wrapped around and use that value instead.
    rb->avail -= written;
    rb_unlock(rb);

    return written == len;
}

inline static size_t do_safe_fread(uint8_t *buf, size_t len, FILE *file) {
    size_t ret = 0;
    while (ret < len && !ferror_unlocked(file) && !feof_unlocked(file)) {
        ret += fread_unlocked(buf, 1, len, file);
    }
    return ret;
}

bool rb_fread_write(ringbuf *rb, size_t len, FILE *file) {
    assert(len <= rb->size);

    while(rb_space(rb) < len) {
        sched_yield();
    }

    size_t writeSize1 = MIN(rb->size - rb->w, len);
    size_t writeSize2 = len - writeSize1;

    size_t read = do_safe_fread(&rb->buf[rb->w], writeSize1, file);
    if (writeSize2) read += do_safe_fread(rb->buf, writeSize2, file);

    rb_lock(rb);
    rb->w = writeSize2 ? writeSize2 : rb->w + writeSize1; // No need to do modulo here, just check if we wrapped around and use that value instead.
    rb->avail += read;
    rb_unlock(rb);

    return read == len;
}


void rb_destroy(ringbuf *rb) {
    pthread_mutex_destroy(&rb->lock);
    free(rb->buf);
    free(rb);
}

void rb_reset(ringbuf *rb) {
    rb_lock(rb);
    rb->r = 0;
    rb->w = 0;
    rb->avail = 0;
    rb_unlock(rb);
}