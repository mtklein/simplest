#include "simplest.h"
#include <stdint.h>
#include <string.h>

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

define_step(grad) {
    half const *alpha = st->ctx;
    Half X = cast(Half, x),
         Y = cast(Half, y);
    return (RGBA){
        X,
        1-X,
        Y,
        splat(Half, *alpha),
    };
}

CC RGBA src(RGBA s, RGBA d) {
    (void)d;
    return s;
}

CC RGBA srcover(RGBA s, RGBA d) {
    s.r += d.r * (1-s.a);
    s.g += d.g * (1-s.a);
    s.b += d.b * (1-s.a);
    s.a += d.a * (1-s.a);
    return s;
}

static Half lerp(Half from, Half to, Half t) {
    return (to - from) * t + from;
}

void blitrow(void* dst, int dx, int dy, int n,
             struct DstFormat const *fmt,
             struct Step            *cov,
             struct Step            *src,
             BlendFn                *blend) {
    int const K = sizeof(Half) / sizeof(half);

    union {
        float arr[8];
        Float vec;
    } iota = {{0,1,2,3,4,5,6,7}};

    Float       x = iota.vec       + (float)dx + 0.5f;
    Float const y = splat(Float,0) + (float)dy + 0.5f;

    while (n >= K) {
        RGBA d = fmt->load(dst),
             s = call(src,x,y),
             b = blend(s,d),
             c = call(cov,x,y),
             l = {
                 lerp(d.r, b.r, c.r),
                 lerp(d.g, b.g, c.g),
                 lerp(d.b, b.b, c.b),
                 lerp(d.a, b.a, c.a),
             };
        fmt->store(l,dst);

        dst = (char*)dst + K*fmt->bpp;
        x  += (float)K;
        n  -= K;
    }
    while (n >= 1) {
        RGBA d = fmt->load1(dst),
             s = call(src,x,y),
             b = blend(s,d),
             c = call(cov,x,y),
             l = {
                 lerp(d.r, b.r, c.r),
                 lerp(d.g, b.g, c.g),
                 lerp(d.b, b.b, c.b),
                 lerp(d.a, b.a, c.a),
             };
        fmt->store1(l,dst);

        dst = (char*)dst + 1*fmt->bpp;
        x  += (float)1;
        n  -= 1;
    }
}

typedef vec(uint32_t) U32;

static RGBA from_rgba_8888(U32 px) {
    return (RGBA) {
        cast(Half, cast(Float, (px >>  0) & 0xff)),
        cast(Half, cast(Float, (px >>  8) & 0xff)),
        cast(Half, cast(Float, (px >> 16) & 0xff)),
        cast(Half, cast(Float, (px >> 24) & 0xff)),
    };
}
static U32 to_rgba_8888(RGBA rgba) {
    return cast(U32, rgba.r * 255.0f + 0.5f) <<  0
         | cast(U32, rgba.g * 255.0f + 0.5f) <<  8
         | cast(U32, rgba.b * 255.0f + 0.5f) << 16
         | cast(U32, rgba.a * 255.0f + 0.5f) << 24;
}

static RGBA load_rgba_8888(void const *ptr) {
    U32 px;
    memcpy(&px, ptr, sizeof px);
    return from_rgba_8888(px);
}
static RGBA load1_rgba_8888(void const *ptr) {
    U32 px={0};
    memcpy(&px, ptr, 4);
    return from_rgba_8888(px);
}
static void store_rgba_8888(RGBA rgba, void *ptr) {
    U32 px = to_rgba_8888(rgba);
    memcpy(ptr, &px, sizeof px);
}
static void store1_rgba_8888(RGBA rgba, void *ptr) {
    U32 px = to_rgba_8888(rgba);
    memcpy(ptr, &px, 4);
}

struct DstFormat const rgba_8888 = {
    4,
    load_rgba_8888,
    load1_rgba_8888,
    store_rgba_8888,
    store1_rgba_8888,
};
