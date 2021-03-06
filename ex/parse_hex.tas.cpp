#include "common.th"

_start:
    prologue

    c <- rel(string_oct)
    d <- 0
    e <- 0
    call(strtol)

    c <- rel(string_dec)
    d <- 0
    e <- 0
    call(strtol)

    c <- rel(string_hex)
    d <- 0
    e <- 0
    call(strtol)

    c <- rel(string_36)
    d <- 0
    e <- 36
    call(strtol)

    c <- rel(string_gbg)
    d <- 0
    e <- 10
    call(strtol)

    c <- rel(string_gbg)
    d <- rel(next)
    e <- 10
    call(strtol)

    c <- [rel(next)]
    d <- rel(next)
    e <- 16
    call(strtol)

    c <- rel(string_gb2)
    d <- rel(next)
    e <- 8
    call(strtol)

    c <- [rel(next)]
    d <- rel(next)
    e <- 36
    call(strtol)

    illegal

next: .word 0

string_dec: .utf32 "123"   ; .word 0
string_oct: .utf32 "0123"  ; .word 0
string_hex: .utf32 "0x123" ; .word 0
string_36:  .utf32 "1Za"   ; .word 0
string_gbg: .utf32 "123CS" ; .word 0
string_gb2: .utf32 "679Z0" ; .word 0
