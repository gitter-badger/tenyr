#include "forth_common.th"

// A little Forth kernel.
// Expects a global symbol called `start' to be defined in another object, which
// should be a list of addresses of code to run, ending with EXIT.

.set link, 0
.global __zero
__zero:
__boot:
    S   <- [reloc(_PSPinit)]
    R   <- [reloc(_RSPinit)]
    push(R,reloc(_done))
    I   <- reloc(start)
    goto(NEXT)

    .global _PSPinit
    .global _RSPinit
_PSPinit:   .word   0x007fffff
_RSPinit:   .word   0x00ffffff

    .global NEXT
NEXT:
    W  <- [I]
    W  <-  rel(W)
    I  <-  I + 1
    X  <- [W]
    jmp(X)

head(ENTER,ENTER):
    .word . + 1
    push(R,I)
    I  <- W + 1
    goto(NEXT)

head(EXIT,EXIT):
    .word . + 1
    pop(R,I)
    goto(NEXT)

_done:
    .word @ABORT

