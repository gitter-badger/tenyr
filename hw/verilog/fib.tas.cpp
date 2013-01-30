// Conventions:
//  O is the stack pointer (post-decrement)
//  B is the (so far only) return register
//  C is the (so far only) argument register
//  N is the relative-jump temp register

#include "common.th"

_start:
    f <- p - .          // base pointer
    c <- 1              // argument
    o <- 1023           // stack pointer

    call(init_display)
    call(disable_cursor)

    j <- 0              // row (0 - 39)
    k <- 0              // column (0 - 3)

loop:
    l <- k * 20         // column (0 - 79)

    // compute fib(N)
    push(c)
    call(fib)
    pop(c)

    c -> [0x100]        // write N to LED
    //c -> [0x101]

    // write N to VGA
    push(c)
    d <- j
    e <- l
    f <- 2
    call(putnum)
    pop(c)

    // write fib(N) to VGA
    push(c)
    c <- b
    d <- j
    e <- l + 4
    f <- 12
    call(putnum)
    pop(c)

    c <- c + 1          // increment N

    j <- j + 1          // increment row
    m <- j > 39         // check for column full
    // TODO try `l <- m * -20 + l`
    k <- k - m          // if j > 39, k <- k - -1
    j <- j &~ m         // if j > 39, j <- j & 0

    goto(loop)
    illegal

fib:
    d <- 1
    d <- c > d          // zero or one ?
    jnzrel(d,_recurse)  // not-zero is true (c >= 2)
    b <- c
    ret

_recurse:
    push(c)
    c <- c - 1
    call(fib)
    pop(c)
    push(b)
    c <- c - 2
    call(fib)
    d <- b
    pop(b)
    b <- d + b

    ret

    // putnum takes number in C, (row,col) in D,E, and field width in F
putnum:
    pushall(h,i,j,k,l,m)
    h <- 0x100
    h <- h << 8         // h is video base
    k <- d * 80
    k <- e + k          // k is offset into display
    k <- k + f          // start at right side of field
    i <- rel(hexes)     // i is base of hex transform

putnumloop:
    m <- h + k + 0x10   // m is k characters past start of text region
    j <- [c & 0xf + i]  // j is character for bottom 4 bits of c
    [m] <- j            // write character to display
    k <- k - 1          // go to the left one character
    c <- c >> 4         // shift down for next iteration
    l <- c == 0         // shall we loop ?
    jzrel(l,putnumloop)

putnumdone:
    popall(h,i,j,k,l,m)
    ret


init_display:
    pushall(h,k,l,m);
    h <- 0x100
    h <- h << 8         // h is video base
    k <- 0              // k is offset into text region

init_display_loop:
    m <- h + k + 0x10   // m is k characters past start of text region
    [m] <- ' '          // write space to display
    k <- k + 1          // go to the right one character
    l <- k > 0x20       // shall we loop ?
    jzrel(l,init_display_loop)

init_display_done:
    popall(h,k,l,m);
    ret


disable_cursor:
    pushall(g,h)
    h <- 0x100
    g <- [h << 8]
    g <- g &~ (1 << 6)  // unset cursor-enable bit
    g -> [h << 8]
    popall(g,h)
    ret


hexes:
    .word '0', '1', '2', '3', '4', '5', '6', '7'
    .word '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'

