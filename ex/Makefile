.SUFFIXES:
.SUFFIXES: .tas .to .texe

TAS = ../tas
TLD = ../tld

CPPFLAGS += -I../lib

vpath %.tas ../lib

TARGETS = hello.texe fib.texe compare.texe sieve.texe maths.texe
CLEANFILES += $(TARGETS)

all: $(TARGETS)

hello.texe: puts.to
compare.texe: strcmp.to puts.to
maths.texe: isqrt.to umod.to udiv.to

%.to: %.tas
	cpp $(CPPFLAGS) $^ | $(TAS) -o$@ -

%.texe: %.to
	$(TLD) -o$@ $^

clean:
	$(RM) $(CLEANFILES)

clobber: clean
	$(RM) *.to
