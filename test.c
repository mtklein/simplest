#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>

struct Point {
    float x,y;
};

struct Multisample {
    struct Point const *offset;
    int                 offsets,unused;
};
static RGBA stage_fn(multisample, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    struct Multisample const *ms = st->ctx;
    RGBA c = {0};
    for (struct Point const *o = ms->offset; o < ms->offset + ms->offsets; o++) {
        RGBA sample = call(st+1, (RGBA_XY){.x=s.x+o->x, .y=s.y+o->y}, d);
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
    half invW,
         invH;
};
static RGBA stage_fn(sample_grad, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    struct Grad const *grad = st->ctx;
    Half a = (Half){0} + 0.875;
    return call(st+1, (RGBA_XY) {
        .r = a * cast(Half, s.x) * grad->invW,
        .g = a * (Half){0} + 0.5,
        .b = a * cast(Half, s.y) * grad->invH,
        .a = a,
    }, d);
}

int main(int argc, char **argv) {
    int const loops = argc > 1 ? atoi(argv[1]) : 1;

    _Bool full = 0;
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp("full", argv[i])) { full = 1; }
    }

    struct Point const offset[] = {
    #if 1
        {-0.125, -0.375},
        {+0.375, -0.125},
        {-0.375, +0.125},
        {+0.125, +0.375},
    #else
        {+1/16.0f, -3/16.0f},
        {-1/16.0f, +3/16.0f},
        {+5/16.0f, +1/16.0f},
        {-3/16.0f, -5/16.0f},

        {-5/16.0f, +5/16.0f},
        {-7/16.0f, -1/16.0f},
        {+3/16.0f, +7/16.0f},
        {+7/16.0f, +7/16.0f},
    #endif
    };
    struct Multisample ms = {.offset=offset, .offsets = sizeof offset / sizeof *offset};

    int const w = 319,
              h = 240;

    float const r = 100;
    struct affine affine = {
        1.000f/r, 0.250f/r, -160.0f/r,
        0.125f/r, 1.000f/r, -120.0f/r,
    };
    struct Stage cover_full[] = {stage_cover_full},
                 cover_oval[] = {{multisample, &ms}, stage_affine(&affine), stage_cover_circle},
                *cover        = full ? cover_full : cover_oval;

    struct Grad grad = {
        (half)1/w,
        (half)1/h
    };
    struct Stage color[] = {
        {sample_grad, &grad},
        stage_swap_rb,
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
            blit_row(px + y*w, 0,y,w, fmt_rgb_fff,cover,color);
        }
    }

    if (loops == 1) {
        stbi_write_hdr("/dev/stdout", w,h,3, &px->r);
    }

    free(px);
    return 0;
}
