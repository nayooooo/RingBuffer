#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "port/port.h"

#ifndef RB_ADDRESS
#define RB_ADDRESS uint32_t
#endif

#define RB_OK                  0
#define RB_ERROR              -1
#define RB_ERROR_PARAM        -2
#define RB_ERROR_INVALID      -3
#define RB_ERROR_LOCKED       -4
#define RB_ERROR_UNLOCKED     -5

typedef enum {
    RINGBUFFER_INVALID_MODE = 0U,
    RINGBUFFER_CPU_MODE     = 1U,
    RINGBUFFER_DMA_MODE     = 1U,
} RingBufferMode;

typedef enum {
    RINGBUFFER_DMA_IDLE     = 0U,
    RINGBUFFER_DMA_READY    = 1U,
    RINGBUFFER_DMA_BUSY     = 2U,
    RINGBUFFER_DMA_ERROR    = 3U,
} RingBufferDMAState;

typedef int (*RINGBUFFER_DMA_CONFIG)(RB_ADDRESS src, RB_ADDRESS det, uint32_t size);
typedef int (*RINGBUFFER_DMA_START)(void);
typedef int (*RINGBUFFER_DMA_STOP)(void);
typedef uint32_t (*RINGBUFFER_DMA_RECVED_LEN)(void);

typedef void (*RINGBUFFER_CLEAN_CHCHE)(uint32_t start_addr, uint32_t size);
typedef void (*RINGBUFFER_INVALID_CHCHE)(uint32_t start_addr, uint32_t size);

typedef struct {
    uint8_t *buff;
    uint32_t size;

    uint32_t head;
    uint32_t tail;
    uint32_t len;

    RingBufferMode mode;
    RingBufferDMAState dmaState;
    RB_ADDRESS srcAddr;
    RB_ADDRESS detAddr;
    uint32_t blockSize;

    uint64_t overflowTimes;
    uint64_t totalIn;
    uint64_t totalOut;

    volatile int inLock;
    volatile int outLock;

    RINGBUFFER_DMA_CONFIG DmaConfig;
    RINGBUFFER_DMA_START DmaStart;
    RINGBUFFER_DMA_STOP DmaStop;
    RINGBUFFER_DMA_RECVED_LEN DmaRecvedLen;

    RINGBUFFER_CLEAN_CHCHE CleanCache;
    RINGBUFFER_INVALID_CHCHE InvalidCache;
} RingBuffer;

int RingBufferCreate(RingBuffer *rb, uint32_t size, RingBufferMode mode);
int RingBufferDelete(RingBuffer *rb);
int RingBufferInit(RingBuffer *rb, uint8_t *buff, uint32_t size, RingBufferMode mode);
int RingBufferDeinit(RingBuffer *rb);

int RingBufferModeSwitchTo(RingBuffer *rb, RingBufferMode mode);

uint32_t RingBufferLenGet(RingBuffer *rb);
uint32_t RingBufferSizeGet(RingBuffer *rb);
uint64_t RingBufferTotalInGet(RingBuffer *rb);
uint64_t RingBufferTotalOutGet(RingBuffer *rb);

uint32_t RingBufferPut(RingBuffer *rb, uint8_t *data, uint32_t len);
uint32_t RingBufferGet(RingBuffer *rb, uint8_t *data, uint32_t size);

int RingBufferDMADeviceRegister(RingBuffer *rb);
int RingBufferDMADeviceUnregister(RingBuffer *rb);

int RingBufferDMAConfig(RingBuffer *rb, RB_ADDRESS src, uint32_t size);
int RingBufferDMAStart(RingBuffer *rb);
int RingBufferDMAStop(RingBuffer *rb);
int RingBufferDMAComplete(RingBuffer *rb);  // Call at dma complete irq

#ifdef __cplusplus
}
#endif

#endif  // !__RINGBUFFER_H__
