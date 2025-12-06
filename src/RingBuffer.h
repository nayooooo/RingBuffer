#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "RingBuffer_cfg.h"

#include "port/port.h"

#ifndef RB_ADDRESS
#if _WIN64
#define RB_ADDRESS uint64_t
#else
#define RB_ADDRESS uint32_t
#endif
#endif

#define RB_OK                  0
#define RB_ERROR              -1
#define RB_ERROR_PARAM        -2
#define RB_ERROR_INVALID      -3
#define RB_ERROR_SYSTEM       -4
#define RB_ERROR_MEMORY       -5
#define RB_ERROR_LOCKED       -6
#define RB_ERROR_UNLOCKED     -7

typedef enum {
    RINGBUFFER_INVALID_MODE = 0U,
    RINGBUFFER_CPU_MODE,
    RINGBUFFER_DMA_MODE,
    RINGBUFFER_MODE_MAX
} RingBufferMode;

typedef enum {
    RINGBUFFER_DMA_ERROR    = 0U,
    RINGBUFFER_DMA_IDLE,
    RINGBUFFER_DMA_READY,
    RINGBUFFER_DMA_BUSY,
} RingBufferDMAState;

#if RINGBUFFER_USE_DMA_MODE

typedef int (*RINGBUFFER_DMA_CONFIG)(RB_ADDRESS src, RB_ADDRESS det, uint32_t size);
typedef int (*RINGBUFFER_DMA_START)(void);
typedef int (*RINGBUFFER_DMA_STOP)(void);
typedef uint32_t (*RINGBUFFER_DMA_RECVED_LEN)(void);

typedef void (*RINGBUFFER_CLEAN_CHCHE)(uint32_t start_addr, uint32_t size);
typedef void (*RINGBUFFER_INVALID_CHCHE)(uint32_t start_addr, uint32_t size);

#endif  /* RINGBUFFER_USE_DMA_MODE */

typedef struct {
    uint8_t *buff;
    uint32_t size;

    volatile uint32_t head;
    volatile uint32_t tail;

    RingBufferMode mode;
    
#if RINGBUFFER_USE_DMA_MODE
    volatile RingBufferDMAState dmaState;
    volatile RB_ADDRESS srcAddr;
    volatile RB_ADDRESS detAddr;
    volatile uint32_t blockSize;

    volatile uint32_t dmaHasCompleted;
#endif  /* RINGBUFFER_USE_DMA_MODE */

#if RINGBUFFER_USE_RX_OVERFLOW
    uint64_t overflowTimes;
#endif  /* RINGBUFFER_USE_RX_OVERFLOW */
    uint64_t totalIn;
    uint64_t totalOut;

    volatile int inLock;
    volatile int outLock;

#if RINGBUFFER_USE_DMA_MODE
    RINGBUFFER_DMA_CONFIG DmaConfig;
    RINGBUFFER_DMA_START DmaStart;
    RINGBUFFER_DMA_STOP DmaStop;
    RINGBUFFER_DMA_RECVED_LEN DmaRecvedLen;

    RINGBUFFER_CLEAN_CHCHE CleanCache;
    RINGBUFFER_INVALID_CHCHE InvalidCache;
#endif  /* RINGBUFFER_USE_DMA_MODE */
} RingBuffer;

uint32_t RingBufferLibraryBit(void);

int RingBufferCreate(RingBuffer *rb, uint32_t size);
int RingBufferDelete(RingBuffer *rb);
int RingBufferInit(RingBuffer *rb, uint8_t *buff, uint32_t size);
int RingBufferDeinit(RingBuffer *rb);

uint32_t RingBufferLenGet(RingBuffer *rb);
uint32_t RingBufferSizeGet(RingBuffer *rb);
uint64_t RingBufferTotalInGet(RingBuffer *rb);
uint64_t RingBufferTotalOutGet(RingBuffer *rb);

uint32_t RingBufferPut(RingBuffer *rb, uint8_t *data, uint32_t size);
uint32_t RingBufferGet(RingBuffer *rb, uint8_t *data, uint32_t size);

#if RINGBUFFER_USE_DMA_MODE

int RingBufferDMADeviceRegister(
    RingBuffer *rb,
    RINGBUFFER_DMA_CONFIG DmaConfig,
    RINGBUFFER_DMA_START DmaStart,
    RINGBUFFER_DMA_STOP DmaStop,
    RINGBUFFER_DMA_RECVED_LEN DmaRecvedLen,
    RINGBUFFER_CLEAN_CHCHE CleanCache,
    RINGBUFFER_INVALID_CHCHE InvalidCache
);
int RingBufferDMADeviceUnregister(RingBuffer *rb);

int RingBufferDMAConfig(RingBuffer *rb, RB_ADDRESS src, uint32_t size);
int RingBufferDMAStart(RingBuffer *rb);
int RingBufferDMAStop(RingBuffer *rb);
int RingBufferDMAComplete(RingBuffer *rb);  // Call at dma complete irq

#endif  /* RINGBUFFER_USE_DMA_MODE */

#ifdef __cplusplus
}
#endif

#endif  // !__RINGBUFFER_H__
