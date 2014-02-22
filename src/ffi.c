#include "sim.h"
#include "ffi.h"
#include "obj.h"
#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif

static int at_pc(struct machine_state *m, void *cud)
{
    int32_t *pc = cud;
    return m->regs[15] == *pc;
}

int tf_run_until(struct sim_state *s, uint32_t start_address, int flags, cont_pred
        stop, void *cud)
{
    int rc = 0;

    if (!(flags & TF_IGNORE_FIRST_PREDICATE) && (rc = stop(&s->machine, cud)))
        return rc;

    do {
        struct element i;
        s->dispatch_op(s, OP_INSN_READ, s->machine.regs[15], &i.insn.u.word);

        if (run_instruction(s, &i))
            return -1;
    } while (!(rc = stop(&s->machine, cud)));

    return rc;
}

int tf_get_addr(const struct sim_state *s, const char *symbol, uint32_t *addr)
{
    int rc = 0;

    // for each object in loaded state
    // for each symbol in object
    // compare name

    *addr = 0; // XXX

    return rc;
}

int tf_call(struct sim_state *s, const char *symbol)
{
    int rc = 0;
    uint32_t addr, nextaddr;
    rc = tf_get_addr(s, symbol, &addr);
    if (rc)
        return rc;

    nextaddr = addr + 1;
    // TODO need to create call shim so we don't just execute one instruction
    // and stop
    rc = tf_run_until(s, addr, 0, at_pc, &nextaddr);

    return rc;
}

