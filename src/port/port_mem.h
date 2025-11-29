#ifndef __PORT_MEM_H__
#define __PORT_MEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#define RB_MEMSET(dst, val, size)           memset(dst, val, size)
#define RB_MEMCPY(dst, src, size)           memcpy(dst, src, size)
#define RB_MEMCMP(buf1, buf2, size)         memcmp(buf1, buf2, size)

#ifdef __cplusplus
}
#endif

#endif  //!__PORT_MEM_H__
