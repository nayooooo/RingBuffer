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

static int RingBufferModeSwitchTo(RingBuffer *rb, RingBufferMode mode)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }
    if (mode >= RINGBUFFER_MODE_MAX) {
        return RB_ERROR_INVALID;
    }
    if (rb->mode == mode) {
        return RB_OK;
    }

    switch (rb->mode) {
        case RINGBUFFER_INVALID_MODE:
        {
            rb->mode = mode;
            break;
        }
        case RINGBUFFER_CPU_MODE:
        {
            rb->mode = mode;
            break;
        }
        case RINGBUFFER_DMA_MODE:
        {
            rb->mode = mode;
            break;
        }
        default:
            return RB_ERROR;
    }

    return RB_OK;
}

#if RINGBUFFER_USE_DMA_MODE

static void _RingBufferDMAModeUpdateLen(RingBuffer *rb)
{
    uint32_t recvedLen = 0;

    if (rb->DmaRecvedLen && rb->dmaState == RINGBUFFER_DMA_BUSY) {
        recvedLen = rb->DmaRecvedLen();
        if (recvedLen > rb->blockSize) {
            return;
        }

        if (_RingBufferInLock(rb) != RB_OK) {
            return;
        }

        rb->tail = (rb->detAddr - (uint32_t)(uintptr_t)&rb->buff[0] + recvedLen) % rb->size;

        _RingBufferInUnlock(rb);
    }
}

#endif  /* RINGBUFFER_USE_DMA_MODE */

int RingBufferCreate(RingBuffer *rb, uint32_t size)
{
    uint8_t *buff = nullptr;

    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }
    if (size <= 0) {
        return RB_ERROR_PARAM;
    }

    buff = (uint8_t *)RB_MALLOC(size);
    if (buff == nullptr) {
        return RB_ERROR_MEMORY;
    }

    return RingBufferInit(rb, buff, size);
}

int RingBufferDelete(RingBuffer *rb)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }

    if (rb->buff) {
        RB_FREE(rb->buff);
    }

    return RingBufferDeinit(rb);
}

int RingBufferInit(RingBuffer *rb, uint8_t *buff, uint32_t size)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }
    if (buff == nullptr || size <= 0) {
        return RB_ERROR_PARAM;
    }

    rb->buff = buff;
    rb->size = size;

    rb->head = 0;
    rb->tail = 0;

    rb->mode = RINGBUFFER_INVALID_MODE;

#if RINGBUFFER_USE_RX_OVERFLOW
    rb->overflowTimes = 0;
#endif  /* RINGBUFFER_USE_RX_OVERFLOW */
    rb->totalIn = 0;
    rb->totalOut = 0;

    rb->inLock = 0;
    rb->outLock = 0;

    return RingBufferModeSwitchTo(rb, RINGBUFFER_CPU_MODE);
}

int RingBufferDeinit(RingBuffer *rb)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }

    rb->buff = nullptr;
    rb->size = 0;

    rb->head = 0;
    rb->tail = 0;

#if RINGBUFFER_USE_RX_OVERFLOW
    rb->overflowTimes = 0;
#endif  /* RINGBUFFER_USE_RX_OVERFLOW */
    rb->totalIn = 0;
    rb->totalOut = 0;

    rb->inLock = 0;
    rb->outLock = 0;

    return RingBufferModeSwitchTo(rb, RINGBUFFER_INVALID_MODE);
}

uint32_t RingBufferLenGet(RingBuffer *rb)
{
    if (rb == nullptr || rb->size <= 0) {
        return 0;
    }

#if RINGBUFFER_USE_DMA_MODE
    if (rb->mode == RINGBUFFER_DMA_MODE) {
        _RingBufferDMAModeUpdateLen(rb);
    }
#endif  /* RINGBUFFER_USE_DMA_MODE */

    return (rb->tail + rb->size - rb->head) % rb->size;
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

uint32_t RingBufferPut(RingBuffer *rb, uint8_t *data, uint32_t size)
{
    uint32_t len;

    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return 0;
    }
    if (rb->mode != RINGBUFFER_CPU_MODE) {
        return 0;
    }
    if (data == nullptr || size <= 0) {
        return 0;
    }

    if (_RingBufferInLock(rb) != RB_OK) {
        return 0;
    }

    len = RingBufferLenGet(rb);

    if (len >= rb->size) {
        _RingBufferInUnlock(rb);
        return 0;
    }

    if (size > rb->size - len - 1) {
        size = rb->size - len - 1;
    }
    if (size == 0) {
        return 0;
    }

    if (rb->tail + size <= rb->size) {
        RB_MEMCPY(&rb->buff[rb->tail], &data[0], size);
        if (rb->CleanCache) {
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[rb->tail], size);
        }
    } else {
        RB_MEMCPY(&rb->buff[rb->tail], &data[0], rb->size - rb->tail);
        RB_MEMCPY(&rb->buff[0], &data[rb->size - rb->tail], size - (rb->size - rb->tail));
        if (rb->CleanCache) {
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[rb->tail], rb->size - rb->tail);
            rb->CleanCache((uint32_t)(uintptr_t)&rb->buff[0], size - (rb->size - rb->tail));
        }
    }
    rb->tail = (rb->tail + size) % rb->size;
    rb->totalIn += size;

    _RingBufferInUnlock(rb);

    return size;
}

uint32_t RingBufferGet(RingBuffer *rb, uint8_t *data, uint32_t size)
{
    uint32_t len;

    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return 0;
    }
    if (data == nullptr || size <= 0) {
        return 0;
    }

    if (_RingBufferOutLock(rb) != RB_OK) {
        return 0;
    }

    if (rb->mode != RINGBUFFER_DMA_MODE) {
        len = RingBufferLenGet(rb);
    } else {
        do {
            if (rb->dmaHasCompleted) {
                rb->dmaHasCompleted = 0;
            }
            len = RingBufferLenGet(rb);
            if (!rb->dmaHasCompleted) {
                break;
            }
        } while (1);
    }

    if (len <= 0) {
        _RingBufferOutUnlock(rb);
        return 0;
    }

    if (size > len) {
        size = len;
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
    rb->totalOut += size;

    _RingBufferOutUnlock(rb);

    return size;
}

#if RINGBUFFER_USE_DMA_MODE

int RingBufferDMADeviceRegister(
    RingBuffer *rb,
    RINGBUFFER_DMA_CONFIG DmaConfig,
    RINGBUFFER_DMA_START DmaStart,
    RINGBUFFER_DMA_STOP DmaStop,
    RINGBUFFER_DMA_RECVED_LEN DmaRecvedLen,
    RINGBUFFER_CLEAN_CHCHE CleanCache,
    RINGBUFFER_INVALID_CHCHE InvalidCache
)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }
    if (DmaConfig == nullptr || DmaStart == nullptr || DmaRecvedLen == nullptr) {
        return RB_ERROR_PARAM;
    }

    rb->dmaState = RINGBUFFER_DMA_IDLE;
    rb->srcAddr = 0;
    rb->detAddr = (uint32_t)(uintptr_t)&rb->buff[0];
    rb->blockSize = 0;

    rb->dmaHasCompleted = 0;

    rb->DmaConfig = DmaConfig;
    rb->DmaStart = DmaStart;
    rb->DmaStop = DmaStop;
    rb->DmaRecvedLen = DmaRecvedLen;
    rb->CleanCache = CleanCache;
    rb->InvalidCache = InvalidCache;

    RingBufferModeSwitchTo(rb, RINGBUFFER_DMA_MODE);

    return RB_OK;
}

int RingBufferDMADeviceUnregister(RingBuffer *rb)
{
    if (rb == nullptr) {
        return RB_ERROR_PARAM;
    }

    RingBufferDMAStop(rb);

    rb->dmaState = RINGBUFFER_DMA_ERROR;
    rb->srcAddr = 0;
    rb->detAddr = 0;
    rb->blockSize = 0;

    rb->dmaHasCompleted = 0;

    rb->DmaConfig = nullptr;
    rb->DmaStart = nullptr;
    rb->DmaStop = nullptr;
    rb->DmaRecvedLen = nullptr;

    rb->CleanCache = nullptr;
    rb->InvalidCache = nullptr;

    RingBufferModeSwitchTo(rb, RINGBUFFER_CPU_MODE);

    return RB_OK;
}

int RingBufferDMAConfig(RingBuffer *rb, RB_ADDRESS src, uint32_t size)
{
    int status;

    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR_PARAM;
    }
    if (src == 0 || size <= 0) {
        return RB_ERROR_PARAM;
    }
    if (rb->mode != RINGBUFFER_DMA_MODE) {
        return RB_ERROR_PARAM;
    }
    if ((rb->dmaState != RINGBUFFER_DMA_READY) && (rb->dmaState != RINGBUFFER_DMA_IDLE)) {
        return RB_ERROR_INVALID;
    }

    rb->srcAddr = src;
    rb->blockSize = size;

    if (rb->DmaConfig) {
        status = rb->DmaConfig(rb->srcAddr, rb->detAddr, rb->blockSize);
        if (status) {
            return status;
        }
    }

    if (rb->dmaState == RINGBUFFER_DMA_IDLE) {
        rb->dmaState = RINGBUFFER_DMA_READY;
    }

    return RB_OK;
}

int RingBufferDMAStart(RingBuffer *rb)
{
    int status;

    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR_PARAM;
    }
    if (rb->dmaState != RINGBUFFER_DMA_READY) {
        return RB_ERROR_INVALID;
    }

    if (rb->DmaStart) {
        status = rb->DmaStart();
        if (status) {
            return status;
        }
    }

    rb->dmaState = RINGBUFFER_DMA_BUSY;

    return RB_OK;
}

int RingBufferDMAStop(RingBuffer *rb)
{
    int status;

    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR_PARAM;
    }
    if (rb->dmaState != RINGBUFFER_DMA_BUSY) {
        return RB_ERROR_INVALID;
    }

    if (rb->DmaStop) {
        status = rb->DmaStop();
        if (status) {
            return status;
        }
    }

    _RingBufferDMAModeUpdateLen(rb);

    rb->dmaState = RINGBUFFER_DMA_READY;

    return RB_OK;
}

int RingBufferDMAComplete(RingBuffer *rb)
{
    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR_PARAM;
    }
    if (rb->mode != RINGBUFFER_DMA_MODE) {
        return RB_ERROR_PARAM;
    }
    if (rb->dmaState != RINGBUFFER_DMA_BUSY) {
        return RB_ERROR_INVALID;
    }

    _RingBufferDMAModeUpdateLen(rb);

    rb->dmaHasCompleted = 1;

    rb->detAddr = (uint32_t)(uintptr_t)&rb->buff[rb->tail];

    rb->dmaState = RINGBUFFER_DMA_READY;

    return RB_OK;
}

#endif  /* RINGBUFFER_USE_DMA_MODE */
