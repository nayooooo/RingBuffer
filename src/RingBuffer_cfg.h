#ifndef __RINGBUFFER_CFG_H__
#define __RINGBUFFER_CFG_H__

#define RINGBUFFER_USE_DMA_MODE           1
#define RINGBUFFER_USE_RX_OVERFLOW        0

#if (RINGBUFFER_USE_RX_OVERFLOW)
#error not support overflow
#endif

#endif  // !__RINGBUFFER_CFG_H__
