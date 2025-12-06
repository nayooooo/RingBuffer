#ifndef __RINGBUFFER_CFG_H__
#define __RINGBUFFER_CFG_H__

#define RINGBUFFER_USE_DMA_MODE           1
#define RINGBUFFER_USE_RX_OVERFLOW        1

#if (RINGBUFFER_USE_RX_OVERFLOW)
#if (!RINGBUFFER_USE_DMA_MODE)
#error overflow must open dma mode
#endif
#endif

#endif  // !__RINGBUFFER_CFG_H__
