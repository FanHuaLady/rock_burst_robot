#ifndef __LWIP_ARCH_CC_H
#define __LWIP_ARCH_CC_H

#include "stm32f4xx.h"
#include <string.h>

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_UNISTD_H   1
#define LWIP_PROVIDE_ERRNO 1

#define X8_F  "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#if defined(__GNUC__)
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#else
#define PACK_STRUCT_BEGIN __packed
#define PACK_STRUCT_STRUCT
#endif
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_FLD_8(x) x
#define PACK_STRUCT_FLD_S(x) x

void LWIP_PlatformAssert(const char *message);

#define LWIP_PLATFORM_DIAG(x)
#define LWIP_PLATFORM_ASSERT(x) LWIP_PlatformAssert(x)

#endif
