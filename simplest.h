#pragma once
#include <stddef.h>
#include <stdint.h>

#if 0
    #if defined(__clang__)
        #pragma clang diagnostic ignored "-Wreserved-macro-identifier"
    #endif
    #undef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#endif


#if defined(__wasm__)
    typedef long fmask;
#else
    typedef int  fmask;
#endif
_Static_assert(sizeof(fmask) == sizeof(float), "");

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    typedef _Float16 half;
    typedef int16_t  hmask;
#else
    typedef float half;
    typedef fmask hmask;
#endif
_Static_assert(sizeof(hmask) == sizeof(half), "");

#if defined(__AVX__)
    #define VS 32
#else
    #define VS 16
#endif

#define K (int)(VS / sizeof(half))

#if defined(__GNUC__) || defined(__clang__)
    #define   vec(T)   T __attribute__((vector_size(K * sizeof(T))))
    #define  cast(T,v) __builtin_convertvector(v,T)
#else
    #define   vec(T)   T
    #define  cast(T,v) (T)(v)
#endif

#if defined(__clang__) && defined(__x86_64__)
    #define CC __regcall
#else
    #define CC
#endif

#if defined(__clang__)
    #define ND __attribute__((nodebug))
#else
    #define ND
#endif


typedef vec(half)    Half;
typedef vec(hmask)   HMask;
typedef vec(float)   Float;
typedef vec(fmask)   FMask;
typedef vec(int32_t) I32;
typedef vec(uint8_t) U8;


typedef struct {
    Half r,g,b,a;
} RGBA;

typedef union {
    struct { Half r,g,b,a; };
    struct { Float x,y; };
} RGBA_XY;

struct Stage {
    RGBA (CC *fn)(struct Stage*, Half,Half,Half,Half, RGBA const*);
    void *ctx;
};

ND static inline RGBA call(struct Stage *st, RGBA_XY s, RGBA const *d) {
    return st->fn(st, s.r,s.g,s.b,s.a, d);
}

#define stage_fn(name, ...)                                                         \
    CC name(struct Stage *st, Half,Half,Half,Half, RGBA const*);                    \
    ND static inline RGBA name##_(struct Stage*, RGBA_XY s, RGBA const *d);         \
    CC RGBA name(struct Stage *st, Half r, Half g, Half b, Half a, RGBA const *d) { \
        return name##_(st, (RGBA_XY){{r,g,b,a}}, d);                                \
    }                                                                               \
    ND static inline RGBA name##_(__VA_ARGS__)

extern struct Stage const stage_white,
                          stage_blend_srcover;

struct affine {
    float sx,kx,tx,
          ky,sy,ty;
};
struct Stage stage_affine(struct affine*);

struct PixelFormat {
    size_t    bpp;
    RGBA (CC *load )(void const*);
    void (CC *store)(RGBA, void*);
};
extern struct PixelFormat const fmt_rgba_8888,
                                fmt_rgb_fff;

void blit_row(void *dst, int dx, int dy, int n,
              struct PixelFormat fmt,
              struct Stage       cover[],
              struct Stage       color[]);
