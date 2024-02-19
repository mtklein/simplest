#include "simplest.h"

#define splat(T,v) (((T){0} + 1) * (v))

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
    Half X = __builtin_convertvector(x,Half),
         Y = __builtin_convertvector(y,Half);
    return (RGBA){
        X,
        1-X,
        Y,
        splat(Half, *alpha),
    };
}
