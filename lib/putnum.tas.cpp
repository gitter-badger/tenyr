#include "common.th"

.global putnum
    // putnum takes number in C, (row,col) in D,E, and field width in F
putnum:
    pushall(h,i,j,k,l,m)
    h <- 0x100
    h <- h << 8         // h is video base
    k <- d * 80 + e     // k is offset into display
    k <- k + f          // start at right side of field
    i <- rel(hexes)     // i is base of hex transform

putnumloop:
    m <- h + k + 0x10   // m is k characters past start of text region
    j <- [c & 0xf + i]  // j is character for bottom 4 bits of c
    [m] <- j            // write character to display
    k <- k - 1          // go to the left one character
    c <- c >> 4         // shift down for next iteration
    l <- c <> 0         // shall we loop ?
    jnzrel(l,putnumloop)

putnumdone:
    popall(h,i,j,k,l,m)
    ret

hexes:
    .word '0', '1', '2', '3', '4', '5', '6', '7'
    .word '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'

