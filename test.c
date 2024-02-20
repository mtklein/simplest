#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #include <arm_neon.h>
#endif

static void write_to_fd(void *ctx, void *buf, int len) {
    write(*(int*)ctx, buf, (size_t)len);
}

CC static RGBA load_zero(void const *ptr) {
    (void)ptr;
    return (RGBA){0};
}
CC static void store_rgb_fff(RGBA rgba, void *ptr) {
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
static struct DstFormat const rgb_fff_ro = {12, load_zero, store_rgb_fff};

struct grad_ctx {
    half invW,
         invH;
};

static define_step(grad) {
    struct grad_ctx const *ctx = st->ctx;
    Half r =  cast(Half, x) * ctx->invW,
         g = splat(Half, 0.5),
         b =  cast(Half, y) * ctx->invH,
         a = splat(Half, 1.0);
    return (RGBA){r,g,b,a};
}


int main(int argc, char **argv) {
    int loops = argc > 1 ? atoi(argv[1]) : 1;

    int const w = 320,
              h = 240;
    struct grad_ctx grad_ctx = {(half)1/w, (half)1/h};

    struct Step cover[] = { {full_coverage, NULL} };
    struct Step color[] = { {grad, &grad_ctx} };

    struct RGB { float r,g,b; } *px = malloc(sizeof *px * (size_t)w * (size_t)h);

    while (loops --> 0) {
        for (int y = 0; y < h; y++) {
            blitrow(px + y*w, 0,y,w, &rgb_fff_ro, cover,color,blend_srcover);
        }
        int fd = 1;
        stbi_write_hdr_to_func(write_to_fd,&fd, w,h,3, &px->r);
    }

    free(px);
    return 0;
}
