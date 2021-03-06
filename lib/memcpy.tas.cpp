#include "common.th"
#include "errno.th"

    .global memcpy
// c,d,e <- dst, src, len
memcpy:
    pushall(f,g)
    b <- c                      // return original value of dst
    f <- 0                      // load offset with 0
L_memcpy_loop:
    g <- e == f                 // check if count is reached
    jnzrel(g,L_memcpy_done)
    g <- [d + f]                // copy src word into temp
    g -> [c + f]                // copy temp into dst word
    f <- f + 1                  // increment offset
    goto(L_memcpy_loop)
L_memcpy_done:
    popall(f,g)
    ret

