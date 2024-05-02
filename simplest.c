#include "simplest.h"
#include <assert.h>
#include <string.h>
#if defined(__ARM_NEON__)
    #include <arm_neon.h>
#endif

static RGBA stage_fn(cover_full, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    (void)st;
    (void)s;
    (void)d;
    Half one = (Half){0} + 1;
    return (RGBA){one,one,one,one};
}
struct Stage const stage_cover_full = {cover_full,NULL};

static RGBA stage_fn(affine, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    struct affine const *m = st->ctx;
    return call(st+1, (RGBA_XY) {
        .x = s.x * m->sx + (s.y * m->kx + m->tx),
        .y = s.x * m->ky + (s.y * m->sy + m->ty),
    }, d);
}
struct Stage stage_affine(struct affine *m) { return (struct Stage){affine,m}; }

static RGBA stage_fn(blend_srcover, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    (void)st;
    return (RGBA) {
        .r = s.r + d.r * (1-s.a),
        .g = s.g + d.g * (1-s.a),
        .b = s.b + d.b * (1-s.a),
        .a = s.a + d.a * (1-s.a),
    };
}
struct Stage const stage_blend_srcover = {blend_srcover,NULL};


CC static RGBA load_rgba_8888(void const *ptr) {
#if 1 && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
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
#if 1 && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
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
struct PixelFormat const fmt_rgba_8888 = {4, load_rgba_8888, store_rgba_8888};

#if defined(__clang__)
    #define for_lane(i) _Pragma("unroll") for (int i = 0; i < K; i++)
#else
    #define for_lane(i)                   for (int i = 0; i < K; i++)
#endif

CC static RGBA load_rgb_fff(void const *ptr) {
    for_lane(i);
    float const *p = ptr;
#if 1 && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    float32x4x3_t lo = vld3q_f32(p+ 0),
                  hi = vld3q_f32(p+12);
    typedef float16x4_t Part;
    return (RGBA) {
        __builtin_shufflevector(cast(Part,lo.val[0]), cast(Part,hi.val[0]), 0,1,2,3,4,5,6,7),
        __builtin_shufflevector(cast(Part,lo.val[1]), cast(Part,hi.val[1]), 0,1,2,3,4,5,6,7),
        __builtin_shufflevector(cast(Part,lo.val[2]), cast(Part,hi.val[2]), 0,1,2,3,4,5,6,7),
        (Half){0} + 1,
    };
#else
    RGBA c = {.a = (Half){0}+1};
    half *r = (half*)&c.r,
         *g = (half*)&c.g,
         *b = (half*)&c.b;
    for_lane(i) {
        *r++ = (half)*p++;
        *g++ = (half)*p++;
        *b++ = (half)*p++;
    }
    return c;
#endif
}

CC static void store_rgb_fff(RGBA rgba, void *ptr) {
    rgba.r *= 1 / rgba.a;
    rgba.g *= 1 / rgba.a;
    rgba.b *= 1 / rgba.a;

    float *p = ptr;
#if 1 && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
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
    for_lane(i) {
        *p++ = (float)*r++;
        *p++ = (float)*g++;
        *p++ = (float)*b++;
    }
#endif
}
struct PixelFormat const fmt_rgb_fff = {12, load_rgb_fff, store_rgb_fff};

enum Coverage { NONE, PARTIAL, FULL };
static enum Coverage classify(RGBA c) {
#if 1 && defined(__clang__)
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

static void blit_slab(void *ptr, RGBA_XY xy,
                      struct PixelFormat fmt, struct Stage cover[], struct Stage color[]) {
    RGBA c = call(cover, xy,xy);
    enum Coverage coverage = classify(c);

    if (coverage != NONE) {
        RGBA d = fmt.load(ptr),
             s = call(color, xy, (RGBA_XY){.r=d.r, .g=d.g, .b=d.b, .a=d.a});
        if (coverage != FULL) {
            s.r = (s.r - d.r) * c.r + d.r;
            s.g = (s.g - d.g) * c.g + d.g;
            s.b = (s.b - d.b) * c.b + d.b;
            s.a = (s.a - d.a) * c.a + d.a;
        }
        fmt.store(s, ptr);
    }
}

void blit_row(void *ptr, int dx, int dy, int n,
              struct PixelFormat fmt, struct Stage cover[], struct Stage color[]) {
    union {
        float arr[8];
        Float vec;
    } const iota = {{0,1,2,3,4,5,6,7}};

    RGBA_XY xy = {
        .x = iota.vec   + (float)dx + 0.5f,
        .y = (Float){0} + (float)dy + 0.5f,
    };

    while (n >= K) {
        blit_slab(ptr,xy,fmt,cover,color);
        ptr   = (char*)ptr + K*fmt.bpp;
        xy.x += (float)K;
        n    -= K;
    }

    if (n) {
        size_t const len = (size_t)n*fmt.bpp;
        float tmp[4*K];
        assert(len <= sizeof(tmp));

        memcpy(tmp,ptr,len);
        blit_slab(tmp,xy,fmt,cover,color);
        memcpy(ptr,tmp,len);
    }
}
