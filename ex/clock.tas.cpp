#include "common.th"

_start:
    bare_metal_init()
    prologue

top:
    d <- [rel(min0)]
    e <- [rel(min1)]
    f <- [rel(sec0)]
    g <- [rel(sec1)]

    h <- d << 4 + e
    h <- h << 4 + f
    h <- h << 4 + g
    h -> [0x100]

    c <- 1
    call(sleep)

    g <- g + 1
    h <- g == 10
    g <- g &~ h
    f <- f - h
    h <- f == 6
    f <- f &~ h
    e <- e - h
    h <- e == 10
    e <- e &~ h
    d <- d - h
    h <- d == 6
    d <- d &~ h

    d -> [rel(min0)]
    e -> [rel(min1)]
    f -> [rel(sec0)]
    g -> [rel(sec1)]
    goto(top)

    illegal

sleep:
    // 40MHz clock, 4-cycle ticks() loop, 10cpi
    c <- c * 1000
    c <- c * 1000
    call(ticks)
    ret

ticks:
    pushall(d,e)
    d <- 0
Lticks_loop:
    e <- d < c
    d <- d + 1
    a <- a // delay to make the loop 4 cycles long
    jnzrel(e,Lticks_loop)
    popall_ret(d,e)

min0: .word 0
min1: .word 0
sec0: .word 0
sec1: .word 0

