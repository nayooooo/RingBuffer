#ifndef __PORT_HEAP_H__
#define __PORT_HEAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#define RB_MALLOC(size)           malloc(size)
#define RB_FREE(ptr)              free(ptr)

#ifdef __cplusplus
}
#endif

#endif  //!__PORT_HEAP_H__
