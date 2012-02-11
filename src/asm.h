#ifndef ASM_H_
#define ASM_H_

#include <stdio.h>

enum { ASM_ASSEMBLE = 1, ASM_DISASSEMBLE = 2 };

struct format {
    const char *name;
    int (*init)(FILE *, int flags, void **ud);
    // TODO combine `in' and `out' functions
    int (*in  )(FILE *, struct instruction *, void *ud);
    int (*out )(FILE *, struct instruction *, void *ud);
    int (*fini)(FILE *, void **ud);
};

int find_format_by_name(const void *_a, const void *_b);

#define ASM_AS_INSN 1
#define ASM_AS_DATA 2

// returns number of characters printed
int print_disassembly(FILE *out, struct instruction *i, int flags);
int print_registers(FILE *out, int32_t regs[16]);

extern const struct format formats[];
extern const size_t formats_count;
#endif

