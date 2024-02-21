#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_to_fd(void *ctx, void *buf, int len) {
    write(*(int*)ctx, buf, (size_t)len);
}

struct grad_ctx {
    half invW,
         invH;
};

static define_stage(grad) {
    struct grad_ctx const *ctx = st->ctx;
    Half r =  cast(Half, x) * ctx->invW,
         g = splat(Half, 0.5),
         b =  cast(Half, y) * ctx->invH,
         a = splat(Half, 1.0);
    return (RGBA){r,g,b,a};
}


int main(int argc, char **argv) {
    int        loops = argc > 1 ? atoi(argv[1]) : 1;
    char const *mode = argc > 2 ?      argv[2]  : "circle";

    int const w = 320,
              h = 240;

    struct RGB { float r,g,b; } *px = calloc((size_t)w * (size_t)h, sizeof *px);
    struct PixelFormat const fmt = {sizeof *px, load_zero, store_rgb_fff};

    struct   grad_ctx   grad_ctx = {(half)1/w, (half)1/h};
    struct circle_ctx circle_ctx = {160,120,100};

    struct Stage color[] = {{swap_rb,NULL}, {grad,&grad_ctx}},
            full_cover[] = {{white, NULL}},
          circle_cover[] = {{circle, &circle_ctx}},
                *cover   = 0 == strcmp(mode, "circle") ? circle_cover : full_cover;

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
