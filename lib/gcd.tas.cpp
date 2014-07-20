#include "common.th"

.global gcd

// Computes GCD(C, D) and stores it in B.
// The variant which uses subtraction will ultimately be a lot cheaper than
// the traditional approach involving mod.
gcd:
  push(k)
  // If C == 0, return D.
  b <- d
  k <- c == 0
  jnzrel(k, done)

  b <- c

loop:
  k <- d == 0
  jnzrel(k, done)

  k <- d < b
  jnzrel(k, else)

  d <- d - b
  goto(loop)

else:
  b <- b - d
  goto(loop)

done:
  pop(k)
  ret
