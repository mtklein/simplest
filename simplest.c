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
