#include "forth_common.th"
#define TEST_LOOKUP 1

.global INBUF .global INPOS .global INLEN
.L_INBUF_before:
INBUF:
#if TEST_LOOKUP
    .utf32 "  words"
#endif
    .utf32 "                                               "
    .utf32 "                                               "
.L_INBUF_after:
INPOS: .word 0
INLEN: .word .L_INBUF_after - .L_INBUF_before

.set link, 0
head(start,start): .word
    @NOOP

top: .word
#if TEST_LOOKUP
    @TIB, @TO_IN, @FETCH, @ADD,
    @BL, @WORD,
    @FIND,
    IFNOT0(found,notfound),
#endif

    @TIB, @DUP, @TO_IN, @SWAP, @SUB,        // tib used
    @IN_LEN, @SWAP, @SUB,                   // tib left
    @ACCEPT                                 // count

strip_spaces: .word
    @TIB, @TO_IN, @FETCH, @ADD,
    @FETCHR, @BL, @CMP_EQ,
    IFNOT0(advance,done_stripping)
advance: .word
    @LITERAL, 1, @CHARS, @TO_IN, @ADDMEM,
    @LITERAL, @strip_spaces, @RELOC, @SET_IP

done_stripping: .word
    @TIB, @TO_IN, @FETCH, @ADD,
    @FETCHR, @EMIT,
    @EXIT
find_word: .word
    @TIB, @TO_IN, @FETCH, @ADD,
    @FIND,
    IFNOT0(found,notfound)
notfound: .word
    // TODO complain
    @LITERAL, 'X', @EMIT,
    @LITERAL, '\n', @EMIT,
    @ABORT

found: .word
    //@DROP, // drop flag for now, assume found
    @DUP,         @EMIT_UNSIGNED, @BL, @EMIT, @LITERAL, ':', @EMIT, @BL, @EMIT, @CR,
    @DUP, @RELOC, @EMIT_UNSIGNED, @BL, @EMIT, @LITERAL, ':', @EMIT, @BL, @EMIT, @CR,
    @RELOC, @EXECUTE,
	//@EXIT,

    // example of a computed branch
    @LITERAL, 0,
    IFNOT0(top,bottom)
bottom: .word
    @NOOP

wordstart: .word
    @KEY,
    @DUP, @LITERAL, '\n', @CMP_EQ,
    IFNOT0(linedone,checkspace)
checkspace: .word
    @DUP, @BL, @CMP_EQ,
    IFNOT0(worddone,regular),
    @NOOP
regular: .word
    @EMIT,
    @LITERAL, @wordstart, @RELOC, @SET_IP
worddone: .word
    @DROP,
    @CR,
    @LITERAL, @wordstart, @RELOC, @SET_IP
linedone: .word
    @CR,
    @EXIT

//------------------------------------------------------------------------------
// TODO explicit echoing
head(ACCEPT,ACCEPT): .word                      // ( c-addr +n1 -- +n2 )
    @ENTER,

    @OVER, @SWAP                                // C-addr c-addr n1
L_ACCEPT_key: .word
    @KEY,                                       // C-addr c-addr n1 c
    @DUP, @LITERAL, '\n', @CMP_EQ,              // C-addr c-addr n1 c flag
    IFNOT0(L_ACCEPT_exit,L_ACCEPT_regular)
L_ACCEPT_regular: .word
    @ROT,                                       // C-addr n1 c c-addr
    @TUCK,                                      // C-addr n1 c-addr c c-addr
    @STOCHR,                                    // C-addr n1 c-addr
    @LITERAL, 1, @CHARS, @ADD,                  // C-addr n1 c-addr++
    @SWAP,                                      // C-addr c-addr n1
    @SUB_1,                                     // C-addr c-addr n1--
    @DUP, @EQZ,                                 // C-addr c-addr n1 flag
    IFNOT0(L_ACCEPT_done,L_ACCEPT_key)
L_ACCEPT_exit: .word
    @DROP                                       // C-addr c-addr n1
L_ACCEPT_done: .word
    @DROP,                                      // C-addr c-addr
    @SWAP, @SUB,                                // n2
    @EXIT

//------------------------------------------------------------------------------

head(IN_LEN,IN_LEN): .word
    @ENTER,
    @LITERAL, @INLEN, @RELOC, @FETCH,
    @EXIT

head(MASK4BITS,MASK4BITS): .word
    @ENTER,
    @LITERAL, 15, @AND,
    @EXIT

head(HEXTABLE,HEXTABLE): .word
    @ENTER,
    @LITERAL, @hexchars, @RELOC,
    @EXIT

hexchars:
    .word '0','1','2','3','4','5','6','7',
          '8','9','A','B','C','D','E','F'

head(TOHEXCHAR,>HEXCHAR): .word
    @ENTER,
    @HEXTABLE, @ADD, @FETCHR,
    @EXIT

head(PUTS,PUTS): .word
    @ENTER,
    // TODO
    @EXIT

head(SET_IP,SET_IP): .word . + 1
    S   <- S + 1
    I   <- [S]
    goto(NEXT)

head(GET_IP,GET_IP): .word . + 1
    I   -> [S]
    S   <- S - 1
    goto(NEXT)

head(GET_PSP,GET_PSP): .word . + 1
    S   -> [S]
    S   <- S - 1
    goto(NEXT)

head(GET_RSP,GET_RSP): .word . + 1
    R   -> [S]
    S   <- S - 1
    goto(NEXT)

head(RELOC,RELOC): .word . + 1
    W   <- [S + 1]
    W   <- rel(W)
    W   -> [S + 1]
    goto(NEXT)

head(SAYTOP,SAYTOP): .word
    @ENTER,
    @DUP, @EMIT_UNSIGNED, @CR,
    @EXIT

