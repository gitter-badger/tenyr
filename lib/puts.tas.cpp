#include "common.th"
#include "serial.th"

// argument in C

    .global puts
puts:
    push(d)
puts_loop:
    b <- [c]            // load word from string
    d <- b == 0         // if it is zero, we are done
    jnzrel(d,puts_done)
    c <- c + 1          // increment index for next time
    emit(b)             // output character to serial device
    goto(puts_loop)
puts_done:
    pop(d)
    ret

// .ascii should be deprecated
    .global puts_ascii
puts_ascii:
    pushall(b,e)
puts_ascii_loop:
    b <- [c]                    // load word from string
    d <- b == 0                 // if it is zero, we are done
    jnzrel(d,puts_ascii_done)
    c <- c + 1                  // increment index for next time
puts_ascii_inner:
    e <- b & 0xff               // mask off top bits
    d <- e == 0                 // compare
    jnzrel(d,puts_ascii_loop)   // skip to next word if zero
    b <- b >>> 8                // shift down next character
    emit(e)                     // output character to serial device
    goto(puts_ascii_inner)      // and continue with same word
puts_ascii_done:
    popall(b,e)
    ret

