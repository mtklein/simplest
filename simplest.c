#include "simplest.h"
#include <stdint.h>
#include <string.h>
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #include <arm_neon.h>
#endif

define_step(noop) {
    return call(st+1,x,y);
}

define_step(swap_rb) {
    RGBA c = call(st+1,x,y);

    Half tmp;
    tmp = c.r;
    c.r = c.b;
    c.b = tmp;
    return c;
}

define_step(full_coverage) {
    (void)st;
    (void)x;
    (void)y;
    Half one = splat(Half, 1);
    return (RGBA){one,one,one,one};
}

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

static Half lerp(Half from, Half to, Half t) {
    return (to - from) * t + from;
}

void blitrow(void *dst, int dx, int dy, int n,
             struct DstFormat const *fmt,
             struct Step             cover[],
             struct Step             color[],
             BlendFn                *blend) {
    union {
        float arr[8];
        Float vec;
    } const iota = {{0,1,2,3,4,5,6,7}};

    Float       x = iota.vec       + (float)dx + 0.5f;
    Float const y = splat(Float,0) + (float)dy + 0.5f;

    while (n > 0) {
        float tmp[4*K];
        void *tgt = n < K ? tmp : dst;

        if (tgt != dst) {
            memcpy(tgt,dst,(size_t)n*fmt->bpp);
        }
        RGBA d = fmt->load(tgt),
             s = blend(call(color,x,y), d),
             c = call(cover,x,y);
        fmt->store((RGBA){
            lerp(d.r, s.r, c.r),
            lerp(d.g, s.g, c.g),
            lerp(d.b, s.b, c.b),
            lerp(d.a, s.a, c.a),
        }, tgt);
        if (tgt != dst) {
            memcpy(dst,tgt,(size_t)n*fmt->bpp);
        }

        dst = (char*)dst + K*fmt->bpp;
        x  += (float)K;
        n  -= K;
    }
}

typedef vec(int32_t) I32;
typedef vec(uint8_t) U8;

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

struct DstFormat const rgba_8888 = {4, load_rgba_8888, store_rgba_8888};
