#include "common.th"

// arguments in C and D
// result in B ; 0 for match, -1 for mismatch

    .global strcmp
strcmp:
    b <- 0              // start with matching
strcmp_loop:
    j <- [c]            // load word from string
    g <- [d]            // load word from string
    h <- j == a         // if it is zero, we are done
    i <- g == a         // if it is zero, we are done
    jnzrel(h,strcmp_done)
    jnzrel(i,strcmp_done)
    c <- c + 1          // increment index for next time
    d <- d + 1          // increment index for next time
    e <- j <> g         // check for mismatch
    b <- b | e          // accumulate mismatches
    goto(strcmp_loop)
strcmp_done:
    e <- j <> g         // check for mismatch
    b <- b | e          // accumulate mismatches
    e <- h <> i         // check for length mismatch
    b <- b | e          // accumulate mismatches
    ret

// arguments in C, D, and E
// result in B ; 0 for match, -1 for mismatch

    .global strncmp
strncmp:
    b <- 0              // start with matching
strncmp_loop:
    h <- e < 1          // check length to go
    jnzrel(h,strncmp_nreached)
    j <- [c]            // load word from string
    g <- [d]            // load word from string
    h <- j == a         // if it is zero, we are done
    i <- g == a         // if it is zero, we are done
    jnzrel(h,strncmp_done)
    jnzrel(i,strncmp_done)
    c <- c + 1          // increment index for next time
    d <- d + 1          // increment index for next time
    e <- e - 1          // decrement length to check
    f <- j <> g         // check for mismatch
    b <- b | f          // accumulate mismatches
    goto(strncmp_loop)
strncmp_done:
    f <- j <> g         // check for mismatch
    b <- b | f          // accumulate mismatches
    f <- h <> i         // check for length mismatch
    b <- b | f          // accumulate mismatches
    ret
strncmp_nreached:
    ret

