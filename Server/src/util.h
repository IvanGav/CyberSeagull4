#pragma once

#include <cstdint>

typedef int8_t I8;
typedef uint8_t U8;
typedef int16_t I16;
typedef uint16_t U16;
typedef int32_t I32;
typedef uint32_t U32;
typedef int64_t I64;
typedef uint64_t U64;
typedef uintptr_t UPtr;
typedef U8 Byte;
typedef float F32;
typedef double F64;
typedef U8 B8;
typedef U32 B32;
typedef U32 Flags32;
typedef U16 Flags16;
typedef U8 Flags8;

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFFULL
#define I8_MAX 0x7F
#define I16_MAX 0x7FFF
#define I32_MAX 0x7FFFFFFF
#define I64_MAX 0x7FFFFFFFFFFFFFFFULL
#define I8_MIN I8(0x80)
#define I16_MIN I16(0x8000)
#define I32_MIN I32(0x80000000)
#define I64_MIN I64(0x8000000000000000LL)
#define F32_SMALL (__builtin_bit_cast(F32, 0x00800000u))
#define F32_LARGE (__builtin_bit_cast(F32, 0x7F7FFFFFu))
#define F32_INF (__builtin_bit_cast(F32, 0x7F800000u))
#define F32_QNAN (__builtin_bit_cast(F32, 0x7FFFFFFFu))
#define F32_SNAN (__builtin_bit_cast(F32, 0x7FBFFFFFu))
#define F64_SMALL (__builtin_bit_cast(F64, 0x0010000000000000ull))
#define F64_LARGE (__builtin_bit_cast(F64, 0x7FEFFFFFFFFFFFFFull))
#define F64_INF (__builtin_bit_cast(F64, 0x7FF0000000000000ull))
#define F64_QNAN (__builtin_bit_cast(F64, 0x7FFFFFFFFFFFFFFFull))
#define F64_SNAN (__builtin_bit_cast(F64, 0x7FF7FFFFFFFFFFFFull))
#define B32_TRUE 1
#define B32_FALSE 0
#define B8_TRUE 1
#define B8_FALSE 0
