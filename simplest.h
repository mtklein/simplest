#pragma once

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    typedef _Float16 half;
#else
    typedef float half;
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define vec(T) T __attribute__((vector_size(sizeof(T) * 16 / sizeof(half))))
    #define cast(T,v) __builtin_convertvector(v,T)
    #define splat(T,v) (((T){0} + 1) * (v))
#else
    #define vec(T) T
    #define cast(T,v) (T)(v)
    #define splat(T,v) (T)(v)
#endif

#if defined(__clang__) && defined(__x86_64__)
    #define CC __regcall
#else
    #define CC
#endif

typedef vec(half)  Half;
typedef vec(float) Float;

typedef struct {
    Half r,g,b,a;
} RGBA;

struct Step {
    RGBA (CC *fn)(struct Step*, Half,Half,Half,Half);
    void *ctx;
};

#define declare_step(name) CC RGBA name(struct Step*, Half,Half,Half,Half)

#define define_step(name)                                               \
    static RGBA name##_(struct Step *ip, Float x, Float y);             \
    CC RGBA name(struct Step *ip, Half xl, Half xh, Half yl, Half yh) { \
        union {                                                         \
            Half  h[4];                                                 \
            Float f[2];                                                 \
        } xy = {{xl,xh,yl,yh}};                                         \
        return name##_(ip, xy.f[0], xy.f[1]);                           \
    }                                                                   \
    static RGBA name##_(struct Step *ip, Float x, Float y)

static inline RGBA next(struct Step *ip, Float x, Float y) {
    union {
        Float f[2];
        Half  h[4];
    } abi = {{x,y}};
    return ip[1].fn(ip+1, abi.h[0], abi.h[1], abi.h[2], abi.h[3]);
}

declare_step(noop);
declare_step(swap_rb);
declare_step(grad);
