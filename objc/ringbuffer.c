#include <stdlib.h>
#include <string.h>
#include <libkern/OSAtomic.h>

#include "ringbuffer.h"

ring_buffer *rb_create(size_t size) {
    ring_buffer *buffer = malloc(sizeof(ring_buffer));
    if (!buffer) return NULL;
    buffer->buf_ptr = malloc(size);
    if (!buffer->buf_ptr) {
        free(buffer);
        return NULL;
    }
    buffer->size = size;
    buffer->write_idx = 0;
    buffer->read_idx = 0;
    return buffer;
}

void rb_free(ring_buffer *buffer) {
    free(buffer->buf_ptr);
    free(buffer);
}

size_t rb_available_to_write(ring_buffer *buffer) {
    return (buffer->size + buffer->read_idx - buffer->write_idx - 1) % buffer->size;
}

size_t rb_available_to_read(ring_buffer *buffer) {
    return (buffer->size + buffer->write_idx - buffer->read_idx) % buffer->size;
}

int rb_write_to_buffer(ring_buffer *buffer, const void *src, size_t len) {
    if (len > rb_available_to_write(buffer)) return -1;
    size_t write_idx = buffer->write_idx;
    if (write_idx + len <= buffer->size) {
        memcpy(buffer->buf_ptr + write_idx, src, len);
    } else {
        size_t d = buffer->size - write_idx;
        memcpy(buffer->buf_ptr + write_idx, src, d);
        memcpy(buffer->buf_ptr, src + d, len - d);
    }
    OSMemoryBarrier();
    buffer->write_idx = (write_idx + len) % buffer->size;
    return 0; 
}

int rb_read_from_buffer(ring_buffer *buffer, void *dest, size_t len) {
    if (len > rb_available_to_read(buffer)) return -1;
    OSMemoryBarrier();
    size_t read_idx = buffer->read_idx;
    if (read_idx + len <= buffer->size) {
        memcpy(dest, buffer->buf_ptr + read_idx, len);
    } else {
        size_t d = buffer->size - read_idx;
        memcpy(dest, buffer->buf_ptr + read_idx, d);
        memcpy(dest + d, buffer->buf_ptr, len - d);
    }
    buffer->read_idx = (read_idx + len) % buffer->size;
    return 0; 
}
