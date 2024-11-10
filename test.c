#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>

#define len(x) (int)( sizeof(x) / sizeof 0[x] )

static RGBA stage_fn(swap_rb, struct Stage st[], RGBA const *d, RGBA_or_XY s) {
    return call(st+1, d, (RGBA_or_XY) {
        .r = s.b,
        .g = s.g,
        .b = s.r,
        .a = s.a,
    });
}

static Half bit_and(Half x, HMask cond) {
    union {Half h; HMask bits;} pun = {x};
    pun.bits &= cond;
    return pun.h;
}
static RGBA stage_fn(cover_circle, struct Stage st[], RGBA const *d, RGBA_or_XY s) {
    (void)st;
    (void)d;
    Half c = bit_and((Half){0} + 1, cast(HMask, s.x*s.x + s.y*s.y < 1));
    return (RGBA){c,c,c,c};
}


struct Point {
    float x,y;
};

struct Multisample {
    struct Point const *offset;
    int                 offsets,unused;
};
static RGBA stage_fn(multisample, struct Stage st[], RGBA const *d, RGBA_or_XY s) {
    struct Multisample const *ms = st->ctx;
    RGBA c = {0};
    for (struct Point const *o = ms->offset; o < ms->offset + ms->offsets; o++) {
        RGBA sample = call(st+1, d, (RGBA_or_XY){.x=s.x+o->x, .y=s.y+o->y});
        c.r += sample.r;
        c.g += sample.g;
        c.b += sample.b;
        c.a += sample.a;
    }
    c.r *= 1 / (half)ms->offsets;
    c.g *= 1 / (half)ms->offsets;
    c.b *= 1 / (half)ms->offsets;
    c.a *= 1 / (half)ms->offsets;
    return c;
}

struct Grad {
    half alpha,
         invW,invH;
};
static RGBA stage_fn(sample_grad, struct Stage st[], RGBA const *d, RGBA_or_XY s) {
    struct Grad const *grad = st->ctx;
    Half a = (Half){0} + grad->alpha;
    return call(st+1, d, (RGBA_or_XY) {
        .r = a * cast(Half, s.x) * grad->invW,
        .g = a * (Half){0} + 0.5,
        .b = a * cast(Half, s.y) * grad->invH,
        .a = a,
    });
}

int main(int argc, char **argv) {
    int const loops = argc > 1 ? atoi(argv[1]) : 1;

    _Bool full  = 0;
    half  alpha = 0.875;
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp("full", argv[i]))   { full = 1; }
        if (0 == strcmp("opaque", argv[i])) { alpha = 1; }
    }

    struct Point const offset[] = {
        {-0.125, -0.375},
        {+0.375, -0.125},
        {-0.375, +0.125},
        {+0.125, +0.375},
    };
    struct Multisample ms = {.offset=offset, .offsets=len(offset)};

    int const w = 319,
              h = 240;

    float const r = 100;
    struct affine affine = {
        1.000f/r, 0.250f/r, -160.0f/r,
        0.125f/r, 1.000f/r, -120.0f/r,
    };
    struct Stage cover_full[] = {stage_white},
                 cover_oval[] = {{multisample, &ms}, stage_affine(&affine), {cover_circle,NULL}};

    struct Grad grad = {
        alpha,
        (half)1/w,
        (half)1/h
    };
    struct Stage color[] = {
        {sample_grad, &grad},
        {swap_rb, NULL},
        stage_blend_srcover,
    };

    struct RGB { float r,g,b; } *px = calloc((size_t)w * (size_t)h, sizeof *px);
    for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
        float const gray = (x&16)==(y&16) ? 0.125f : 0.5f;
        px[y*w + x] = (struct RGB){gray,gray,gray};
    }

    for (int i = 0; i < loops; i++) {
        for (int y = 0; y < h; y++) {
            blit_row(px + y*w, 0,y,w, fmt_rgb_fff,
                     full ? cover_full : cover_oval,
                     color);
        }
    }

    if (loops == 1) {
        stbi_write_hdr("/dev/stdout", w,h,3, &px->r);
    }

    free(px);
    return 0;
}
