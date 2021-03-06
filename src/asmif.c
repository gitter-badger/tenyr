#include "asmif.h"
#include "ops.h"
// obj.h is included for RLC_* flags ; reconsider their location
#include "obj.h"
#include "parser.h"
#include "parser_global.h"
#include "expr.h"
#include "lexer.h"
#include "common.h"
#include "asm.h"

#include <assert.h>
#include <stdlib.h>
#include <getopt.h>
#include <search.h>
#include <string.h>
#include <strings.h>

#define version() "tas version " STR(BUILD_NAME)

static int add_relocation(struct parse_data *pd, const char *name,
        struct element *insn, int width, int flags);

struct symbol *symbol_find(struct symbol_list *list, const char *name)
{
    list_foreach(symbol_list, elt, list)
        if (!strncmp(elt->symbol->name, name, SYMBOL_LEN))
            return elt->symbol;

    return NULL;
}

// symbol_lookup returns 1 on success
static int symbol_lookup(struct parse_data *pd, struct symbol_list *list, const
        char *name, int32_t *result)
{
    struct symbol *symbol = NULL;
    if ((symbol = symbol_find(list, name))) {
        if (result) {
            if (symbol->ce) {
                if (symbol->name && !strcmp(symbol->name, name))
                    fatal(0, "Bad use before definition of symbol `%s'", name);

                struct element_list **prev = symbol->ce->deferred;
                struct element *c = (prev && *prev) ? (*prev)->elem : NULL;
                return ce_eval(pd, c, symbol->ce, 0, 0, result);
            } else {
                *result = symbol->reladdr;
            }
        }
        return 1;
    }

    // unresolved symbols get a zero value, but this is still success in CE_EXT
    // case (not in CE_SYM case)
    if (result) *result = 0;
    return 0;
}

// add_relocation returns 1 on success
static int add_relocation(struct parse_data *pd, const char *name,
        struct element *insn, int width, int flags)
{
    struct reloc_list *node = calloc(1, sizeof *node);

    if (name) {
        strcopy(node->reloc.name, name, sizeof node->reloc.name);
    } else {
        memset(node->reloc.name, '\0', sizeof node->reloc.name);
    }
    node->reloc.insn  = insn;
    node->reloc.width = width;
    node->reloc.flags = flags;

    node->next = pd->relocs;
    pd->relocs = node;

    if (insn)
        insn->reloc = &node->reloc;

    return 1;
}

static int sym_reloc_handler(struct parse_data *pd, struct element *context,
        int flags, struct const_expr *ce, int width)
{
    int rlc_flags = 0;

    if (flags & RHS_NEGATE)
        rlc_flags |= RLC_NEGATE;

    switch (ce->type) {
        case CE_SYM:
        case CE_EXT:
            if (ce->symbol && ce->symbol->ce) {
                return context ? add_relocation(pd, NULL, context, width, rlc_flags) : 0;
            } else if (ce->type == CE_EXT) {
                const char *name = ce->symbol ? ce->symbol->name : ce->symbolname;
                const char *n = (flags & NO_NAMED_RELOC) ? NULL : name;
                return add_relocation(pd, n, context, width, rlc_flags);
            }
        case CE_ICI:
            return add_relocation(pd, NULL, context, width, rlc_flags);
        default:
            return 0;
    }

    return 0;
}

int ce_eval_const(struct parse_data *pd, struct const_expr *ce,
        int32_t *result)
{
    int rc = ce_eval(pd, NULL, ce, 0, 0, result);
    if (rc == 0) {
        ce->i = *result;
        ce->flags |= DONE_EVAL;
    }
    return rc;
}

// ce_eval should be idempotent. returns 1 on fully-successful evaluation, 0 on incomplete evaluation
int ce_eval(struct parse_data *pd, struct element *context,
        struct const_expr *ce, int flags, int width, int32_t *result)
{
    int32_t left, right;
    int relocate = (flags & DO_RELOCATION) != 0;
    if (flags & DONE_EVAL) {
        *result = ce->i;
        return 0; // No need to re-evaluate
    }

    switch (ce->type) {
        case CE_SYM:
        case CE_EXT:
            if (ce->symbol && ce->symbol->ce) {
                struct element_list *deferred = *ce->symbol->ce->deferred;
                if (!deferred)
                    return 0; // cannot evaluate yet
                struct element *dc = deferred->elem;
                return ce_eval(pd, dc, ce->symbol->ce, flags, width, result)
                    || (relocate ? sym_reloc_handler(pd, dc, flags, ce, width) : 0);
            } else {
                const char *name = ce->symbol ? ce->symbol->name : ce->symbolname;
                int found   = symbol_lookup(pd, pd->symbols, name, result);
                int hflags  = flags | (found ? NO_NAMED_RELOC : 0);
                int handled = relocate ? sym_reloc_handler(pd, context, hflags, ce, width) : 0;
                return found || handled;
            }
        case CE_ICI:
            *result = context ? context->insn.reladdr : 0;
            if (relocate)
                return sym_reloc_handler(pd, context, flags, ce, width);
            return !!context;
        case CE_IMM: *result = ce->i; return 1;
        case CE_OP1:
            if (ce_eval(pd, context, ce->left, flags, width, result)) {
                switch (ce->op) {
                    case '-': *result = -*result; return 1;
                    case '~': *result = ~*result; return 1;
                    default : fatal(0, "Unrecognised const_expr op '%c' (%#x)", ce->op, ce->op);
                }
            }
            return 0;
        case CE_OP2: {
            int rhsflags = flags;
            if (ce->op == '-')
                rhsflags ^= RHS_NEGATE;
            if (ce_eval(pd, context, ce->left ,    flags, width, &left ) &&
                ce_eval(pd, context, ce->right, rhsflags, width, &right))
            {
                switch (ce->op) {
                    case '+' : *result = left +  right; return 1;
                    case '-' : *result = left -  right; return 1;
                    case '*' : *result = left *  right; return 1;
                    case '^' : *result = left ^  right; return 1;
                    case '&' : *result = left &  right; return 1;
                    case '|' : *result = left |  right; return 1;
                    case LSH : *result = left << right; return 1;
                    case RSHA: *result = left >> right; return 1;
                    case RSH : *result = ((uint32_t)left) >> right; return 1;
                    case '/' : {
                        if (right != 0)
                            *result = left / right;
                        else
                            fatal(0, "Constant expression attempted %d/%d", left, right);
                        return 1;
                    }
                    default : fatal(0, "Unrecognised const_expr op '%c' (%#x)", ce->op, ce->op);
                }
            }
            return 0;
        }
        default:
            fatal(0, "Unrecognised const_expr type %d", ce->type);
            return 0;
    }
}

static void ce_free(struct const_expr *ce, int recurse)
{
    if (!ce)
        return;

    if (recurse)
        switch (ce->type) {
            case CE_EXT:
            case CE_SYM:
                if (ce->symbol && ce->symbol->ce) {
                    ce_free(ce->symbol->ce, recurse);
                    ce->symbol->ce = NULL;
                }
                break;
            case CE_ICI:
                break;
            case CE_IMM:
                free(ce->left);
                break;
            case CE_OP2:
                ce_free(ce->right, recurse); /* FALLTHROUGH */
            case CE_OP1:
                ce_free(ce->left, recurse);
                break;
            default:
                fatal(0, "Unrecognised const_expr type %d", ce->type);
        }

    free(ce);
}

static int fixup_deferred_exprs(struct parse_data *pd)
{
    int rc = 0;

    list_foreach(deferred_expr, r, pd->defexprs) {
        struct const_expr *ce = r->ce;

        int32_t result;
        if (ce_eval(pd, ce->insn, ce, DO_RELOCATION, r->width, &result)) {
            result *= r->mult;

            // XXX handle too-large values in a 32-bit field
            // .word 0x123456123456 # this fails to provoke an error
            const char *sstr = (r->width < 32) ? "signed " : "";
            if (!(ce->flags & IGNORE_WIDTH) && result != (int32_t)SEXTEND32(r->width, result)) {
                debug(0, "Expression resulting in value %#x is too large for "
                        "%d-bit %simmediate field", result, r->width, sstr);
                rc |= 1;
            }

            uint32_t mask = -1ll << r->width;
            *r->dest &= mask;
            *r->dest |= result & ~mask;
            ce_free(ce, 1);
        } else {
            fatal(0, "Error while fixing up deferred expressions");
            // TODO print out information about the deferred expression
        }

        free(r);
    }

    return rc;
}

static int mark_globals(struct symbol_list *symbols, struct global_list *globals)
{
    struct symbol *which;
    list_foreach(global_list, g, globals)
        if ((which = symbol_find(symbols, g->name)))
            which->global = 1;

    return 0;
}

static int check_symbols(struct symbol_list *symbols)
{
    int rc = 0;

    // check for and reject duplicates
    void *tree = NULL;
    list_foreach(symbol_list, Node, symbols) {
        if (!Node->symbol->unique)
            continue;

        const char **name = tsearch(Node->symbol->name, &tree, (cmp*)strcmp);

        if (*name != Node->symbol->name) {
            rc = 1;
            break;
        }
    }

    // delete from tree what we added to it
    list_foreach(symbol_list, Node, symbols) {
        if (!tree) break;
        tdelete(Node->symbol, &tree, (cmp*)strcmp);
        Node = Node->next;
    }

    return rc;
}

static int assembly_cleanup(struct parse_data *pd)
{
    list_foreach(element_list, Node, pd->top) {
        free(Node->elem);
        free(Node);
    }

    list_foreach(symbol_list, Node, pd->symbols) {
        if (Node->symbol) {
            ce_free(Node->symbol->ce, 1);
            free(Node->symbol->name);
        }
        free(Node->symbol);
        free(Node);
    }

    list_foreach(global_list, Node, pd->globals)
        free(Node);

    list_foreach(reloc_list, Node, pd->relocs)
        free(Node);

    return 0;
}

static int assembly_fixup_insns(struct parse_data *pd)
{
    int32_t reladdr = 0;
    // first pass, fix up addresses
    list_foreach(element_list, il, pd->top) {
        if (!il->elem)
            continue;

        il->elem->insn.reladdr = reladdr;

        list_foreach(symbol, l, il->elem->symbol) {
            if (!l->resolved) {
                l->reladdr = reladdr;
                l->resolved = 1;
            }
        }

        reladdr += il->elem->insn.size;
    }

    list_foreach(symbol_list, li, pd->symbols)
        list_foreach(symbol, l, li->symbol)
            if (!l->resolved)
                if (ce_eval(pd, NULL, l->ce, DO_RELOCATION, WORD_BITWIDTH, &l->reladdr))
                    l->resolved = 1;

    return 0;
}

static int assembly_inner(struct parse_data *pd, FILE *out, const struct format *f, void *ud)
{
    assembly_fixup_insns(pd);

    mark_globals(pd->symbols, pd->globals);
    if (check_symbols(pd->symbols))
        fatal(0, "Error in symbol processing : check for duplicate symbols");

    if (!fixup_deferred_exprs(pd)) {
        list_foreach(element_list, Node, pd->top)
            // if !Node->elem, it's a placeholder or some kind of dummy
            if (Node->elem)
                f->out(out, Node->elem, ud);

        if (f->sym) {
            list_foreach(symbol_list, Node, pd->symbols) {
                int flags = 0;
                struct symbol *sym = Node->symbol;
                if (sym->ce && (sym->ce->flags & IS_DEFERRED) == 0)
                    flags |= RLC_ABSOLUTE;

                f->sym(out, sym, flags, ud);
            }
        }

        if (f->reloc)
            list_foreach(reloc_list, Node, pd->relocs)
                f->reloc(out, &Node->reloc, ud);
    } else {
        fatal(0, "Error while fixing up deferred expressions");
    }

    assembly_cleanup(pd);

    return 0;
}

int do_assembly(FILE *in, FILE *out, const struct format *f, void *ud)
{
    struct parse_data _pd = {
        .top = NULL,
        .lexstate.savep = {
            _pd.lexstate.saveline[0],
            _pd.lexstate.saveline[1],
        },
    }, *pd = &_pd;

    tenyr_lex_init(&pd->scanner);
    tenyr_set_extra(pd, pd->scanner);

    if (in)
        tenyr_set_in(in, pd->scanner);

    int result = tenyr_parse(pd);
    if (pd->errored)
        debug(0, "Encountered %d error%s while parsing, bailing",
                pd->errored, &"s"[pd->errored == 1]);
    if (!result && !pd->errored && f)
        assembly_inner(pd, out, f, ud);
    tenyr_lex_destroy(pd->scanner);

    return result || pd->errored;
}

int do_disassembly(FILE *in, FILE *out, const struct format *f, void *ud, int flags)
{
    int rc = 0;

    struct element i;
    while ((rc = f->in(in, &i, ud)) >= 0) {
        if (rc == 0)
            continue; // allow a format to emit no instructions
        int len = print_disassembly(out, &i, ASM_AS_INSN | flags);
        if (!(flags & ASM_QUIET)) {
            fprintf(out, "%*s# ", 30 - len, "");
            print_disassembly(out, &i, ASM_AS_DATA | flags);
            fprintf(out, " ; ");
            print_disassembly(out, &i, ASM_AS_CHAR | flags);
            fprintf(out, " ; .addr 0x%08x\n", i.insn.reladdr);
        } else {
            fputc('\n', out);
            // This probably means we want line-oriented output
            fflush(out);
        }
    }

    return f->err ? f->err(ud) : 0;
}

/* vi:set ts=4 sw=4 et: */
