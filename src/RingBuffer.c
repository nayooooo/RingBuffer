#include "RingBuffer.h"

#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr ((void *)0)
#endif
#endif

uint32_t RingBufferLibraryBit(void)
{
#if _WIN64
    return 64;
#elif _WIN32
    return 32;
#else
    return 0;
#endif
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

        rb->tail = (rb->detAddr - (RB_ADDRESS)&rb->buff[0] + recvedLen) % rb->size;
    }
}

#if RINGBUFFER_USE_RX_OVERFLOW

static int _RingBufferDMAModeCheckOverflow(RingBuffer *rb)
{
    if (rb->totalIn - rb->totalOut > rb->size) {
        return 1;
    } else {
        return 0;
    }
}

#endif  /* RINGBUFFER_USE_RX_OVERFLOW */

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

    rb->dataHasPut = 0;

    rb->mode = RINGBUFFER_INVALID_MODE;

#if RINGBUFFER_USE_RX_OVERFLOW
    rb->overflowTimes = 0;
#endif  /* RINGBUFFER_USE_RX_OVERFLOW */
    rb->totalIn = 0;
    rb->totalOut = 0;

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

    rb->dataHasPut = 0;

#if RINGBUFFER_USE_RX_OVERFLOW
    rb->overflowTimes = 0;
#endif  /* RINGBUFFER_USE_RX_OVERFLOW */
    rb->totalIn = 0;
    rb->totalOut = 0;

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

uint64_t RingBufferOverflowTimesGet(RingBuffer *rb)
{
    if (rb == nullptr) {
        return 0;
    }

    return rb->overflowTimes;
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

    len = RingBufferLenGet(rb);

    if (len >= rb->size) {
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
            rb->CleanCache((RB_ADDRESS)&rb->buff[rb->tail], size);
        }
    } else {
        RB_MEMCPY(&rb->buff[rb->tail], &data[0], rb->size - rb->tail);
        RB_MEMCPY(&rb->buff[0], &data[rb->size - rb->tail], size - (rb->size - rb->tail));
        if (rb->CleanCache) {
            rb->CleanCache((RB_ADDRESS)&rb->buff[rb->tail], rb->size - rb->tail);
            rb->CleanCache((RB_ADDRESS)&rb->buff[0], size - (rb->size - rb->tail));
        }
    }
    rb->tail = (rb->tail + size) % rb->size;
    rb->totalIn += size;

    rb->dataHasPut = 1;

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

#if RINGBUFFER_USE_LATEST_LEN
    do {
        if (rb->dataHasPut) {
            rb->dataHasPut = 0;
        }
#endif  /* RINGBUFFER_USE_LATEST_LEN */
        len = RingBufferLenGet(rb);
#if RINGBUFFER_USE_LATEST_LEN
        if (!rb->dataHasPut) {
            break;
        }
    } while (1);
#endif  /* RINGBUFFER_USE_LATEST_LEN */

    if (len <= 0) {
        return 0;
    }

    if (size > len) {
        size = len;
    }

    if (rb->head + size <= rb->size) {
        if (rb->InvalidCache) {
            rb->InvalidCache((RB_ADDRESS)&rb->buff[rb->head], size);
        }
        RB_MEMCPY(&data[0], &rb->buff[rb->head], size);
    } else {
        if (rb->InvalidCache) {
            rb->InvalidCache((RB_ADDRESS)&rb->buff[rb->head], rb->size - rb->head);
            rb->InvalidCache((RB_ADDRESS)&rb->buff[0], size - (rb->size - rb->head));
        }
        RB_MEMCPY(&data[0], &rb->buff[rb->head], rb->size - rb->head);
        RB_MEMCPY(&data[rb->size - rb->head], &rb->buff[0], size - (rb->size - rb->head));
    }
    rb->head = (rb->head + size) % rb->size;
    rb->totalOut += size;

    return size;
}

#if RINGBUFFER_USE_DMA_MODE

#include <stdio.h>
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
    if (DmaConfig == nullptr || DmaRecvedLen == nullptr) {
        return RB_ERROR_PARAM;
    }

    rb->dmaState = RINGBUFFER_DMA_IDLE;
    rb->srcAddr = 0;
    rb->detAddr = (RB_ADDRESS)&rb->buff[0];
    rb->blockSize = 0;

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
    uint32_t len;

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

    rb->dataHasPut = 1;

    if (rb->DmaRecvedLen) {
        len = rb->DmaRecvedLen();
        if (len < rb->blockSize) {
            rb->totalIn += len;
#if RINGBUFFER_USE_RX_OVERFLOW
            if (_RingBufferDMAModeCheckOverflow(rb)) {
                rb->overflowTimes++;
            }
#endif
        }
    }

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

    rb->dataHasPut = 1;

    rb->detAddr = (RB_ADDRESS)&rb->buff[rb->tail];

    rb->totalIn += rb->blockSize;
#if RINGBUFFER_USE_RX_OVERFLOW
    if (_RingBufferDMAModeCheckOverflow(rb)) {
        rb->overflowTimes++;
    }
#endif

    rb->dmaState = RINGBUFFER_DMA_READY;

    return RB_OK;
}

uint32_t RingBufferTailToRightBorderLenGet(RingBuffer *rb)
{
    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR;
    }

    return (rb->size - rb->tail);
}

int RingBufferDataCrossedRightBorder(RingBuffer *rb)
{
    if (rb == nullptr || rb->buff == nullptr || rb->size <= 0) {
        return RB_ERROR;
    }

    return (rb->tail < rb->head);
}

#endif  /* RINGBUFFER_USE_DMA_MODE */
