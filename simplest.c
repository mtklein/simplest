#include "simplest.h"
#include <string.h>
#if defined(__ARM_NEON__)
    #include <arm_neon.h>
#endif

static CC RGBA noop(struct Stage st[], Float x, Float y) {
    return st[1].fn(st+1, x,y);
}
struct Stage const stage_noop = {noop,NULL};

static CC RGBA white(struct Stage st[], Float x, Float y) {
    (void)st;
    (void)x;
    (void)y;
    Half one = (Half){0} + 1;
    return (RGBA){one,one,one,one};
}
struct Stage const stage_white = {white,NULL};

static CC RGBA swap_rb(struct Stage st[], Float x, Float y) {
    RGBA c = st[1].fn(st+1, x,y);

    Half tmp;
    tmp = c.r;
    c.r = c.b;
    c.b = tmp;
    return c;
}
struct Stage const stage_swap_rb = {swap_rb,NULL};

static Float bit_and(Float x, FMask cond) {
    union {Float f; FMask bits;} pun = {x};
    pun.bits &= cond;
    return pun.f;
}

static CC RGBA circle(struct Stage st[], Float x, Float y) {
    (void)st;
    Half c = cast(Half, bit_and((Float){0} + 1, x*x + y*y < 1));
    return (RGBA){c,c,c,c};
}
struct Stage const stage_circle = {circle,NULL};

static CC RGBA affine(struct Stage st[], Float x, Float y) {
    struct affine const *ctx = st->ctx;
    Float X = x * ctx->sx + (y * ctx->kx + ctx->tx),
          Y = x * ctx->ky + (y * ctx->sy + ctx->ty);
    return st[1].fn(st+1, X,Y);
}
struct Stage stage_affine(struct affine *ctx) { return (struct Stage){affine,ctx}; }

CC RGBA blend_src(RGBA s, RGBA d) {
    (void)d;
    return s;
}

CC RGBA blend_srcover(RGBA s, RGBA d) {
    s.r += d.r * (1-s.a);
    s.g += d.g * (1-s.a);
    s.b += d.b * (1-s.a);
    s.a += d.a * (1-s.a);
    return s;
}


CC static RGBA load_rgba_8888(void const *ptr) {
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    uint8x8x4_t px = vld4_u8(ptr);
    return (RGBA) {
        cast(Half, px.val[0]) * (half)(1/255.0),
        cast(Half, px.val[1]) * (half)(1/255.0),
        cast(Half, px.val[2]) * (half)(1/255.0),
        cast(Half, px.val[3]) * (half)(1/255.0),
    };
#else
    I32 px;
    memcpy(&px, ptr, sizeof px);
    return (RGBA) {
        cast(Half, (px >>  0) & 255) * (half)(1/255.0),
        cast(Half, (px >>  8) & 255) * (half)(1/255.0),
        cast(Half, (px >> 16) & 255) * (half)(1/255.0),
        cast(Half, (px >> 24) & 255) * (half)(1/255.0),
    };
#endif
}
CC static void store_rgba_8888(RGBA rgba, void *ptr) {
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    vst4_u8(ptr, ((uint8x8x4_t){
        cast(U8, rgba.r * 255 + 0.5),
        cast(U8, rgba.g * 255 + 0.5),
        cast(U8, rgba.b * 255 + 0.5),
        cast(U8, rgba.a * 255 + 0.5),
    }));
#else
    I32 px = cast(I32, rgba.r * 255 + 0.5) <<  0
           | cast(I32, rgba.g * 255 + 0.5) <<  8
           | cast(I32, rgba.b * 255 + 0.5) << 16
           | cast(I32, rgba.a * 255 + 0.5) << 24;
    memcpy(ptr, &px, sizeof px);
#endif
}
struct PixelFormat const rgba_8888 = {4, load_rgba_8888, store_rgba_8888};

CC RGBA load_zero(void const *ptr) {
    (void)ptr;
    return (RGBA){0};
}

CC void store_rgb_fff(RGBA rgba, void *ptr) {
    float *p = ptr;
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    Float r = cast(Float, rgba.r),
          g = cast(Float, rgba.g),
          b = cast(Float, rgba.b);
    vst3q_f32(p+ 0, ((float32x4x3_t) {
        __builtin_shufflevector(r,r, 0,1,2,3),
        __builtin_shufflevector(g,g, 0,1,2,3),
        __builtin_shufflevector(b,b, 0,1,2,3),
    }));
    vst3q_f32(p+12, ((float32x4x3_t) {
        __builtin_shufflevector(r,r, 4,5,6,7),
        __builtin_shufflevector(g,g, 4,5,6,7),
        __builtin_shufflevector(b,b, 4,5,6,7),
    }));
#else
    half const *r = (half const*)&rgba.r,
               *g = (half const*)&rgba.g,
               *b = (half const*)&rgba.b;
    for (int i = 0; i < K; i++) {
        *p++ = (float)*r++;
        *p++ = (float)*g++;
        *p++ = (float)*b++;
    }
#endif
}

enum Coverage { NONE, PARTIAL, FULL };
static enum Coverage classify(RGBA c) {
#if defined(__clang__)
    if (0 == __builtin_reduce_max(
                __builtin_elementwise_max(__builtin_elementwise_max(c.r, c.g),
                                          __builtin_elementwise_max(c.b, c.a)))) {
        return NONE;
    }
    if (1 ==__builtin_reduce_min(
                __builtin_elementwise_min(__builtin_elementwise_min(c.r, c.g),
                                          __builtin_elementwise_min(c.b, c.a)))) {
        return FULL;
    }
#else
    // TODO
    (void)c;
#endif
    return PARTIAL;
}

void blit_row(void *ptr, int dx, int dy, int n,
              struct PixelFormat const *fmt,
              BlendFn                  *blend,
              struct Stage              cover[],
              struct Stage              color[]) {
    union {
        float arr[8];
        Float vec;
    } const iota = {{0,1,2,3,4,5,6,7}};

    Float       x = iota.vec   + (float)dx + 0.5f;
    Float const y = (Float){0} + (float)dy + 0.5f;

    while (n > 0) {
        RGBA c = cover->fn(cover, x,y);
        enum Coverage coverage = classify(c);

        if (coverage != NONE) {
            float tmp[4*K];
            void *dst = n < K ? tmp : ptr;

            if (dst != ptr) {
                memcpy(dst,ptr,(size_t)n*fmt->bpp);
            }

            RGBA d = fmt->load(dst),
                 s = blend(color->fn(color, x,y), d);
            if (coverage != FULL) {
                s.r = (s.r - d.r) * c.r + d.r;
                s.g = (s.g - d.g) * c.g + d.g;
                s.b = (s.b - d.b) * c.b + d.b;
                s.a = (s.a - d.a) * c.a + d.a;
            }
            fmt->store(s, dst);

            if (dst != ptr) {
                memcpy(ptr,dst,(size_t)n*fmt->bpp);
            }
        }

        ptr = (char*)ptr + K*fmt->bpp;
        x  += (float)K;
        n  -= K;
    }
}
