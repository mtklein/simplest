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
    (void)ip;
    Half X = __builtin_convertvector(x,Half),
         Y = __builtin_convertvector(y,Half);
    return (RGBA){
        X,
        1-X,
        Y,
        (Half){0} + 1,
    };
}
