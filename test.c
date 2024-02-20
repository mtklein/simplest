#include "simplest.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <unistd.h>

static void write_to_fd(void *ctx, void *buf, int len) {
    write(*(int*)ctx, buf, (size_t)len);
}

int main(void) {
    struct RGB { float r,g,b; };

    int const w = 320,
              h = 240;

    struct RGB *px = malloc(sizeof *px * (size_t)w * (size_t)h);

    for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
        px[y*w + x] = (struct RGB){(float)x / w, 0.5, (float)y / h};
    }

    int fd = 1;
    stbi_write_hdr_to_func(write_to_fd,&fd, w,h,3, &px->r);

    free(px);
    return 0;
}
