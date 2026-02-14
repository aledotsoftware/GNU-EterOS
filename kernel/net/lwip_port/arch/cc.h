#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <types.h>
#include <net/defs.h>
#include <serial.h>
#include <string.h>
#include <hal.h>

/* Prevent inclusion of standard headers */
#define LWIP_NO_STDDEF_H 1
#define LWIP_NO_STDINT_H 1
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H 1
#define LWIP_NO_CTYPE_H 1
#define LWIP_NO_UNISTD_H 1

/* Prevent lwIP from redefining byte order macros */
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

/* Types */
typedef uint8_t     u8_t;
typedef int8_t      s8_t;
typedef uint16_t    u16_t;
typedef int16_t     s16_t;
typedef uint32_t    u32_t;
typedef int32_t     s32_t;
typedef uintptr_t   mem_ptr_t;
typedef intptr_t    ptrdiff_t;
typedef u32_t       sys_prot_t;

/* Critical sections */
#define sys_arch_protect() (hal_interrupts_disable(), 0)
#define sys_arch_unprotect(x) hal_interrupts_enable()

/* Format specifiers */
#define U16_F "d"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

/* Byte Order */
#define BYTE_ORDER  LITTLE_ENDIAN

/* Diagnostic Output */
/* For now, just print a static string to avoid varargs complexity without vsnprintf */
#define LWIP_PLATFORM_DIAG(x) do { serial_write_string("[LWIP] "); serial_write_string("\n"); } while(0)

/* Assert */
#define LWIP_PLATFORM_ASSERT(x) do { serial_write_string("[LWIP] ASSERT: "); serial_write_string(x); serial_write_string("\n"); } while(0)

/* Random (Simulated) */
#define LWIP_RAND() ((u32_t)sys_now())

/* Packed structs */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_FIELD(x) x

#endif /* __ARCH_CC_H__ */
