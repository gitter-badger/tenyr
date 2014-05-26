#include "sim.h"
#include "common.h"

#include <assert.h>
#include <stdlib.h>

static void do_op(enum op op, int type, int32_t *rhs, uint32_t X, uint32_t Y,
        uint32_t I)
{
    int32_t  Xs = X;
    uint32_t Xu = X;
    int32_t  Ys = Y;
    int32_t  Is = SEXTEND32(12, I);

    int32_t  O = (type == 0) ? Ys : Is;
    int32_t  A = (type == 0) ? Is : Ys;

    switch (op) {
        case OP_ADD                 : *rhs =  (Xs  +  O) + A; break;
        case OP_SUBTRACT            : *rhs =  (Xs  -  O) + A; break;
        case OP_MULTIPLY            : *rhs =  (Xs  *  O) + A; break;

        case OP_SHIFT_LEFT          : *rhs =  (Xu  << O) + A; break;
        case OP_SHIFT_RIGHT_LOGICAL : *rhs =  (Xu  >> O) + A; break;

        case OP_COMPARE_LT          : *rhs = -(Xs  <  O) + A; break;
        case OP_COMPARE_EQ          : *rhs = -(Xs  == O) + A; break;
        case OP_COMPARE_GT          : *rhs = -(Xs  >  O) + A; break;
        case OP_COMPARE_NE          : *rhs = -(Xs  != O) + A; break;

        case OP_BITWISE_AND         : *rhs =  (Xu  &  O) + A; break;
        case OP_BITWISE_ANDN        : *rhs =  (Xu  & ~O) + A; break;
        case OP_BITWISE_OR          : *rhs =  (Xu  |  O) + A; break;
        case OP_BITWISE_XOR         : *rhs =  (Xu  ^  O) + A; break;
        case OP_BITWISE_XORN        : *rhs =  (Xu  ^ ~O) + A; break;

        default:
            fatal(0, "Encountered reserved opcode");
    }
}

static int do_common(struct sim_state *s, int32_t *Z, int32_t *rhs,
        uint32_t *value, int deref_lhs, int deref_rhs, int reversed)
{
    int read_mem  = reversed ? deref_lhs : deref_rhs;
    int write_mem = reversed ? deref_rhs : deref_lhs;

    int32_t *r    = reversed ? Z         : rhs;
    int32_t *w    = reversed ? rhs       : Z;

    if (read_mem) {
        if (s->dispatch_op(s, OP_DATA_READ, *r, value)) {
            if (s->conf.pause) getchar();
            if (s->conf.abort) abort();
        }
    } else
        *value = *r;

    if (write_mem) {
        if (s->dispatch_op(s, OP_WRITE, *w, value)) {
            if (s->conf.pause) getchar();
            if (s->conf.abort) abort();
        }
    } else if (w != &s->machine.regs[0])  // throw away write to reg 0
        *w = *value;

    return 0;
}

int run_instruction(struct sim_state *s, struct element *i)
{
    int32_t *ip = &s->machine.regs[15];

    ++*ip;

    switch (i->insn.u._xxxx.t) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7: {
            struct instruction_general *g = &i->insn.u._0xxx;
            int32_t rhs = 0;
            uint32_t value;

            do_op(g->op, g->p, &rhs, s->machine.regs[g->x],
                                     s->machine.regs[g->y],
                                     g->imm);
            do_common(s, &s->machine.regs[g->z], &rhs, &value, g->dd == 2,
                    g->dd & 1, g->dd == 3);

            break;
        }
        default:
            if (s->conf.pause) getchar();
            if (s->conf.abort) abort();
            return 1;
    }

    return 0;
}

int run_sim(struct sim_state *s, struct run_ops *ops)
{
    while (1) {
        struct element i;
        if (s->dispatch_op(s, OP_INSN_READ, s->machine.regs[15], &i.insn.u.word)) {
            if (s->conf.pause) getchar();
            if (s->conf.abort) abort();
        }

        if (ops->pre_insn)
            ops->pre_insn(s, &i);

        if (run_instruction(s, &i))
            return 1;

        if (ops->post_insn)
            ops->post_insn(s, &i);
    }
}

int load_sim(op_dispatcher *dispatch_op, void *sud, const struct format *f,
        FILE *in, int load_address)
{
    void *ud = NULL;
    if (f->init)
        f->init(in, ASM_DISASSEMBLE, &ud);

    struct element i;
    while (f->in(in, &i, ud) > 0) {
        // TODO stop assuming addresses are contiguous and monotonic
        if (dispatch_op(sud, OP_WRITE, load_address++, &i.insn.u.word))
            return -1;
    }

    if (f->fini)
        f->fini(in, &ud);

    return 0;
}

