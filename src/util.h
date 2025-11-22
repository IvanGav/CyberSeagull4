#pragma once

#include <cstdint>

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

#define ALIGN_HIGH(num, alignment) (((num) + (static_cast<decltype(num)>(alignment) - 1)) & ~(static_cast<decltype(num)>(alignment) - 1))

//F32 clamp01(F32 f) {
//    return std::max(std::min(0.0f, f), 1.0f);
//}

#pragma pack(push, 1)
struct RGBA8 {
    U8 r, g, b, a;

    B32 operator==(const RGBA8& other) {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    glm::vec4 to_v4f32() {
        F32 scale = 1.0F / 255.0F;
        return glm::vec4{ F32(r) * scale, F32(g) * scale, F32(b) * scale, F32(a) * scale };
    }
    RGBA8 to_rgba8() {
        return *this;
    }
};
//RGBA8 to_rgba8() {
//    return RGBA8{ U8(clamp01(x) * 255.0F), U8(clamp01(y) * 255.0F), U8(clamp01(z) * 255.0F), U8(clamp01(w) * 255.0F) };
//}
struct RGB8 {
    U8 r, g, b;

    B32 operator==(const RGB8& other) {
        return r == other.r && g == other.g && b == other.b;
    }
    RGB8 operator+(RGB8 other) {
        return RGB8{ U8(r + other.r), U8(g + other.g), U8(b + other.b) };
    }
    RGB8 operator-(RGB8 other) {
        return RGB8{ U8(r - other.r), U8(g - other.g), U8(b - other.b) };
    }
    RGBA8 to_rgba8() {
        return RGBA8{ r, g, b, 255 };
    }
};
struct RG8 {
    U8 r, g;

    B32 operator==(RG8 other) {
        return r == other.r && g == other.g;
    }
    RGBA8 to_rgba8() {
        return RGBA8{ r, 0, 0, g };
    }
};
struct R8 {
    U8 r;

    B32 operator==(R8 other) {
        return r == other.r;
    }
    RGBA8 to_rgba8() {
        return RGBA8{ r, 0, 0, 255 };
    }
};
#pragma pack(pop)