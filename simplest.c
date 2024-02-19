#include "simplest.h"

define_step(noop) {
    return next(ip,x,y);
}

define_step(swap_rb) {
    RGBA c = next(ip,x,y);

    Half tmp;
    tmp = c.r;
    c.r = c.b;
    c.b = tmp;
    return c;
}

define_step(grad) {
    half const *alpha = ip->ctx;
    Half X = cast(Half, x),
         Y = cast(Half, y);
    return (RGBA){
        X,
        1-X,
        Y,
        splat(Half, *alpha),
    };
}
