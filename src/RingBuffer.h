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

typedef struct {
    uint8_t *buff;
    uint32_t size;
} RingBuffer;

int RingBufferCreate(RingBuffer *rb, uint32_t size);
int RingBufferDelete(RingBuffer *rb);
int RingBufferInit(RingBuffer *rb, uint8_t *buff, uint32_t size);
int RingBufferDeinit(RingBuffer *rb);

uint32_t RingBufferPut(RingBuffer *rb, uint8_t *data, uint32_t len);
uint32_t RingBufferGet(RingBuffer *rb, uint8_t *data, uint32_t size);

int RingBufferDMADeviceRegister(RingBuffer *rb);
int RingBufferDMADeviceUnregister(RingBuffer *rb);

int RingBufferDMAConfig(RingBuffer *rb, RB_ADDRESS src, RB_ADDRESS det, uint32_t size);
int RingBufferDMAStart(RingBuffer *rb);
int RingBufferDMAStop(RingBuffer *rb);

#ifdef __cplusplus
}
#endif

#endif  // !__RINGBUFFER_H__
