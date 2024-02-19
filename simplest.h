#pragma once

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    typedef _Float16 half;
#else
    typedef float half;
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define vec(T) T __attribute__((vector_size(sizeof(T) * 16 / sizeof(half))))
#else
    #define vec(T) T
#endif

typedef vec(half)  Half;
typedef vec(float) Float;

typedef struct {
    Half r,g,b,a;
} RGBA;

struct Step {
    RGBA (*fn)(struct Step*, Half,Half,Half,Half);
    void *ctx;
};

#define declare_step(name) RGBA name(struct Step*, Half,Half,Half,Half)

#define define_step(name)                                            \
    static RGBA name##_(struct Step *ip, Float x, Float y);          \
    RGBA name(struct Step *ip, Half xl, Half xh, Half yl, Half yh) { \
        union {                                                      \
            Half  h[4];                                              \
            Float f[2];                                              \
        } xy = {{xl,xh,yl,yh}};                                      \
        return name##_(ip, xy.f[0], xy.f[1]);                        \
    }                                                                \
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
