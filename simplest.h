#pragma once
#include <stddef.h>
#include <stdint.h>

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

#if defined(__wasm__)
    typedef long fmask;
#else
    typedef int  fmask;
#endif
_Static_assert(sizeof(fmask) == sizeof(float), "");

typedef vec(half)    Half;
typedef vec(float)   Float;
typedef vec(fmask)   FMask;
typedef vec(int32_t) I32;
typedef vec(uint8_t) U8;


typedef struct {
    Half r,g,b,a;
} RGBA;

struct Stage {
    RGBA (CC *fn)(struct Stage*, Float, Float);
    void *ctx;
};

extern struct Stage const stage_noop,
                          stage_white,
                          stage_swap_rb,
                          stage_circle;

struct affine {
    float sx,kx,tx,
          ky,sy,ty;
};
struct Stage stage_affine(struct affine*);

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

void blit_row(void *dst, int dx, int dy, int n,
              struct PixelFormat const *fmt,
              BlendFn                  *blend,
              struct Stage              cover[],
              struct Stage              color[]);
