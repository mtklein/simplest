#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_to_fd(void *fd, void *buf, int len) {
    write(*(int*)fd, buf, (size_t)len);
}

struct grad {
    half invW,
         invH;
};

static stage_fn(grad, struct Stage st[], Float x, Float y) {
    struct grad const *grad = st->ctx;
    Half r = cast(Half, x) * grad->invW,
         g = (Half){0} + 0.5,
         b = cast(Half, y) * grad->invH,
         a = (Half){0} + 1.0;
    return (RGBA){r,g,b,a};
}
static struct Stage stage_grad(struct grad *ctx) {
    return (struct Stage){grad,ctx};
}


int main(int argc, char **argv) {
    int const loops = argc > 1 ? atoi(argv[1]) : 1;

    _Bool full = 0;
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp("full", argv[i])) { full = 1; }
    }

    int const w = 320,
              h = 240;

    struct RGB { float r,g,b; } *px = calloc((size_t)w * (size_t)h, sizeof *px);
    struct PixelFormat const fmt = {sizeof *px, load_zero, store_rgb_fff};

    float const r = 100;
    struct affine affine = {
        1.000f/r, 0.250f/r, -160.0f/r,
        0.125f/r, 1.000f/r, -120.0f/r,
    };
    struct Stage full_cover[] = {stage_white},
                 oval_cover[] = {stage_affine(&affine), stage_circle},
                     *cover   = full ? full_cover : oval_cover;

    struct grad grad = {(half)1/w, (half)1/h};
    struct Stage color[] = {stage_swap_rb, stage_grad(&grad)};

    for (int i = 0; i < loops; i++) {
        for (int y = 0; y < h; y++) {
            blit_row(px + y*w, 0,y,w, &fmt,blend_srcover,cover,color);
        }
    }

    if (loops == 1) {
        int fd = 1;
        stbi_write_hdr_to_func(write_to_fd,&fd, w,h,3, &px->r);
    }

    free(px);
    return 0;
}
