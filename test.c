#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_to_fd(void *fd, void *buf, int len) {
    write(*(int*)fd, buf, (size_t)len);
}

struct grad { half invW, invH; };
static RGBA stage_fn(grad, struct Stage st[], RGBA_XY s, RGBA_XY d) {
    struct grad const *grad = st->ctx;
    return call(st+1, (RGBA_XY) {
        .r = cast(Half, s.x) * grad->invW,
        .g = (Half){0} + 0.5,
        .b = cast(Half, s.y) * grad->invH,
        .a = (Half){0} + 1.0,
    }, d);
}
static struct Stage stage_grad(struct grad *ctx) { return (struct Stage){grad,ctx}; }


int main(int argc, char **argv) {
    int const loops = argc > 1 ? atoi(argv[1]) : 1;

    _Bool full = 0;
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp("full", argv[i])) { full = 1; }
    }

    int const w = 319,
              h = 240;

    float const r = 100;
    struct affine affine = {
        1.000f/r, 0.250f/r, -160.0f/r,
        0.125f/r, 1.000f/r, -120.0f/r,
    };
    struct Stage cover_full[] = {stage_cover_full},
                 cover_oval[] = {stage_affine(&affine), stage_cover_circle},
                *cover        = full ? cover_full : cover_oval;

    struct grad grad = { (half)1/w, (half)1/h };
    struct Stage color[] = {
        stage_grad(&grad),
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
        int fd = 1;
        stbi_write_hdr_to_func(write_to_fd,&fd, w,h,3, &px->r);
    }

    free(px);
    return 0;
}
