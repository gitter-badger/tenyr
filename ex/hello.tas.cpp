#include "common.th"

_start:
    o <- -1             // set up stack pointer
    c <- rel(hi)        // string starts at @hi
    call(puts)
    illegal

hi:
    .ascii "hello, world"
    .word 0             // mark end of string with a zero

