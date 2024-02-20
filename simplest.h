#pragma once
#include <stddef.h>

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    typedef _Float16 half;
#else
    typedef float half;
#endif

#if defined(__AVX__)
    #define VS 32
#else
    #define VS 16
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define vec(T) T __attribute__((vector_size(sizeof(T) * VS / sizeof(half))))
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
#define K (int)(sizeof(Half) / sizeof(half))

typedef struct {
    Half r,g,b,a;
} RGBA;

struct Stage {
    RGBA (CC *fn)(struct Stage*, Half,Half,Half,Half);
    void *ctx;
};

#define declare_stage(name) CC RGBA name(struct Stage*, Half,Half,Half,Half)

#define define_stage(name)                                                \
    declare_stage(name);                                                  \
    static RGBA name##_(struct Stage st[], Float x, Float y);             \
    CC RGBA name(struct Stage st[], Half xl, Half xh, Half yl, Half yh) { \
        union {                                                           \
            Half  h[4];                                                   \
            Float f[2];                                                   \
        } xy = {{xl,xh,yl,yh}};                                           \
        return name##_(st, xy.f[0], xy.f[1]);                             \
    }                                                                     \
    static RGBA name##_(struct Stage st[], Float x, Float y)

static inline RGBA call(struct Stage st[], Float x, Float y) {
    union {
        Float f[2];
        Half  h[4];
    } abi = {{x,y}};
    return st->fn(st, abi.h[0], abi.h[1], abi.h[2], abi.h[3]);
}

declare_stage(noop);
declare_stage(swap_rb);
declare_stage(white);

typedef RGBA (CC BlendFn)(RGBA, RGBA);

BlendFn blend_src,
        blend_srcover;

struct PixelFormat {
    size_t bpp;
    RGBA (CC *load )(void const*);
    void (CC *store)(RGBA, void*);
};
extern struct PixelFormat const rgba_8888;

CC RGBA load_zero(void const*);
CC void store_rgb_fff(RGBA, void*);

void blitrow(void *dst, int dx, int dy, int n,
             struct PixelFormat const *fmt,
             BlendFn                  *blend,
             struct Stage              cover[],
             struct Stage              color[]);
