#include "RingBuffer.h"

#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr ((void *)0)
#endif
#endif

#ifndef uintptr_t
typedef unsigned long long uintptr_t;
#endif

static int _RingBufferInLock(RingBuffer *rb)
{
    if (rb->inLock < 0) {
        return RB_ERROR_INVALID;
    }
    if (rb->inLock > 0) {
        return RB_ERROR_LOCKED;
    }
    rb->inLock++;
    return RB_OK;
}

static int _RingBufferInUnlock(RingBuffer *rb)
{
    if (rb->inLock < 0) {
        return RB_ERROR_INVALID;
    }
    if (rb->inLock == 0) {
        return RB_ERROR_UNLOCKED;
    }
    rb->inLock--;
    return RB_OK;
}

static int _RingBufferOutLock(RingBuffer *rb)
{
    if (rb->outLock < 0) {
        return RB_ERROR_INVALID;
    }
    if (rb->outLock > 0) {
        return RB_ERROR_LOCKED;
    }
    rb->outLock++;
    return RB_OK;
}

static int _RingBufferOutUnlock(RingBuffer *rb)
{
    if (rb->outLock < 0) {
        return RB_ERROR_INVALID;
    }
    if (rb->outLock == 0) {
        return RB_ERROR_UNLOCKED;
    }
    rb->outLock--;
    return RB_OK;
}

static void _RingBufferDMAModeUpdateLen(RingBuffer *rb)
{
    uint32_t recvedLen = 0;

    if (rb->DmaRecvedLen && rb->dmaState == RINGBUFFER_DMA_BUSY) {
        recvedLen = rb->DmaRecvedLen();

        if (_RingBufferInLock(rb) != RB_OK) {
            return;
        }

        rb->tail = (rb->detAddr + recvedLen) % rb->size;
        rb->len = (rb->tail + rb->size - rb->head) % rb->size;

        _RingBufferInUnlock(rb);
    }
}

uint32_t RingBufferLenGet(RingBuffer *rb)
{
    int status;

    if (rb == nullptr) {
        return 0;
    }

    if (rb->mode == RINGBUFFER_DMA_MODE) {
        _RingBufferDMAModeUpdateLen(rb);
    }

    return rb->len;
}

uint32_t RingBufferSizeGet(RingBuffer *rb)
{
    if (rb == nullptr) {
        return 0;
    }

    return rb->size;
}

uint64_t RingBufferTotalInGet(RingBuffer *rb)
{
    if (rb == nullptr) {
        return 0;
    }

    return rb->totalIn;
}

uint64_t RingBufferTotalOutGet(RingBuffer *rb)
{
    if (rb == nullptr) {
        return 0;
    }

    return rb->totalOut;
}

uint32_t RingBufferPut(RingBuffer *rb, uint8_t *data, uint32_t len)
{
    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return 0;
    }
    if (rb->mode != RINGBUFFER_CPU_MODE) {
        return 0;
    }
    if (data == nullptr || len <= 0) {
        return 0;
    }

    if (_RingBufferInLock(rb) != RB_OK) {
        return 0;
    }

    if (rb->len >= rb->size) {
        _RingBufferInUnlock(rb);
        return 0;
    }

    if (len > rb->size - rb->len) {
        len = rb->size - rb->len;
    }

    if (rb->tail + len <= rb->size) {
        RB_MEMCPY(&rb->buff[rb->tail], &data[0], len);
        if (rb->CleanCache) {
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[rb->tail], len);
        }
    } else {
        RB_MEMCPY(&rb->buff[rb->tail], &data[0], rb->size - rb->tail);
        RB_MEMCPY(&rb->buff[0], &data[rb->size - rb->tail], len - (rb->size - rb->tail));
        if (rb->CleanCache) {
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[rb->tail], rb->size - rb->tail);
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[0], len - (rb->size - rb->tail));
        }
    }
    rb->tail = (rb->tail + len) % rb->size;
    rb->len += len;

    _RingBufferInUnlock(rb);

    return len;
}

uint32_t RingBufferGet(RingBuffer *rb, uint8_t *data, uint32_t size)
{
    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return 0;
    }
    if (data == nullptr || size <= 0) {
        return 0;
    }

    if (_RingBufferOutLock(rb) != RB_OK) {
        return 0;
    }

    if (rb->len <= 0) {
        _RingBufferOutUnlock(rb);
        return 0;
    }

    if (rb->mode == RINGBUFFER_DMA_MODE) {
        _RingBufferDMAModeUpdateLen(rb);
    }

    if (size > rb->len) {
        size = rb->len;
    }

    if (rb->head + size <= rb->size) {
        if (rb->InvalidCache) {
            rb->InvalidCache((uint32_t)(uintptr_t)&rb->buff[rb->head], size);
        }
        RB_MEMCPY(&data[0], &rb->buff[rb->head], size);
    } else {
        if (rb->InvalidCache) {
            rb->InvalidCache((uint32_t)(uintptr_t)&rb->buff[rb->head], rb->size - rb->head);
            rb->InvalidCache((uint32_t)(uintptr_t)&rb->buff[0], size - (rb->size - rb->head));
        }
        RB_MEMCPY(&data[0], &rb->buff[rb->head], rb->size - rb->head);
        RB_MEMCPY(&data[rb->size - rb->head], &rb->buff[0], size - (rb->size - rb->head));
    }

    rb->head = (rb->head + size) % rb->size;
    rb->len -= size;

    _RingBufferOutUnlock(rb);

    return size;
}
