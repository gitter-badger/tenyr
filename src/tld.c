#include "obj.h"
// for RAM_BASE
#include "devices/ram.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <search.h>
#include <string.h>
#include <strings.h>

struct link_state {
    UWord addr;     ///< current address
    int obj_count;
    struct obj_list {
        struct obj *obj;
        int i;
        struct obj_list *next;
    } *objs, **next_obj;
    struct obj *relocated;

    long insns, syms, rlcs, words;
};

struct defn {
    char name[SYMBOL_LEN];
    struct obj *obj;
    UWord reladdr;
    UWord flags;

    struct link_state *state;   ///< state reference used for twalk() support
};

struct objmeta {
    struct obj *obj;
    int size;
    int offset;

    struct link_state *state;   ///< state reference used for twalk() support
};

static const char shortopts[] = "o:hV";

static const struct option longopts[] = {
    { "output"      , required_argument, NULL, 'o' },

    { "help"        ,       no_argument, NULL, 'h' },
    { "version"     ,       no_argument, NULL, 'V' },

    { NULL, 0, NULL, 0 },
};

#define version() "tld version " STR(BUILD_NAME)

static int usage(const char *me)
{
    printf("Usage: %s [ OPTIONS ] image-file [ image-file ... ] \n"
           "Options:\n"
           "  -o, --output=X        write output to filename X\n"
           "  -h, --help            display this message\n"
           "  -V, --version         print the string `%s'\n"
           , me, version());

    return 0;
}

static int do_load(struct link_state *s, FILE *in)
{
    int rc = 0;
    struct obj_list *node = calloc(1, sizeof *node);

    struct obj *o = calloc(1, sizeof *o);
    rc = obj_read(o, in);
    node->obj = o;
    node->i = s->obj_count++;
    // put the objects on the list in order
    node->next = NULL;
    *s->next_obj = node;
    s->next_obj = &node->next;

    return rc;
}

static int do_unload(struct link_state *s)
{
    list_foreach(obj_list,ol,s->objs) {
        obj_free(ol->obj);
        free(ol);
    }

    return 0;
}

static int ptrcmp(const void *a, const void *b)
{
    return *(const char**)a - *(const char**)b;
}

static int do_link_build_state(struct link_state *s, void **objtree, void **defns)
{
    // running offset, tracking where to pack objects tightly one after another
    UWord running = 0;

    // read in all symbols
    list_foreach(obj_list, Node, s->objs) {
        struct obj *i = Node->obj;

        if (!i->rec_count) {
            debug(0, "Object has no records, skipping");
            continue;
        }

        if (i->rec_count != 1)
            debug(0, "Object has more than one record, only using first");

        struct objmeta *meta = calloc(1, sizeof *meta);
        meta->state = s;
        meta->obj = i;
        meta->size = i->records[0].size;
        meta->offset = running;
        running += i->records[0].size;
        struct objmeta **look = tsearch(meta, objtree, ptrcmp);
        if (*look != meta)
            fatal(0, "Duplicate object `%p'", (*look)->obj);

        list_foreach(objsym, sym, i->symbols) {
            struct defn *def = calloc(1, sizeof *def);
            def->state = s;
            strcopy(def->name, sym->name, sizeof def->name);
            def->obj = i;
            def->reladdr = sym->value;
            def->flags = sym->flags;

            struct defn **look = tsearch(def, defns, (cmp*)strcmp);
            if (*look != def)
                fatal(0, "Duplicate definition for symbol `%s'", def->name);
        }
    }

    return 0;
}

static int do_link_relocate_obj_reloc(struct obj *i, struct objrlc *rlc,
                                       void **objtree, void **defns)
{
    UWord reladdr = 0;

    // TODO support more than one record per object
    if (i->rec_count > 1)
        fatal(0, "Object has more than one record, unsupported");
    else if (i->rec_count < 1)
        fatal(0, "Object has invalid record count, aborting");

    struct objrec *r = &i->records[0];
    if (rlc->addr < r->addr ||
        rlc->addr - r->addr > r->size)
    {
        debug(0, "Invalid relocation @ 0x%08x outside record @ 0x%08x size %d",
              rlc->addr, r->addr, r->size);
        return 1;
    }

    struct objmeta **me = tfind(&i, objtree, ptrcmp);
    if (rlc->name[0]) {
        struct defn def;
        strcopy(def.name, rlc->name, sizeof def.name);
        struct defn **look = tfind(&def, defns, (cmp*)strcmp);
        if (!look)
            fatal(0, "Missing definition for symbol `%s'", rlc->name);
        reladdr = (*look)->reladdr;

        if (((*look)->flags & RLC_ABSOLUTE) == 0) {
            struct objmeta **it = tfind(&(*look)->obj, objtree, ptrcmp);
            reladdr += (*it)->offset ;
        }
    } else {
        // this is a null relocation ; it just wants us to update the
        // offset
        reladdr = (*me)->offset;
        // negative null relocations invert the value of the offset
        if (rlc->flags & RLC_NEGATE)
            reladdr = -reladdr;
    }
    // here we actually add the found-symbol's value to the relocation
    // slot, being careful to trim to the right width
    UWord *dest = &r->data[rlc->addr - r->addr];
    UWord mask = (((1 << (rlc->width - 1)) << 1) - 1);
    UWord updated = (*dest + reladdr) & mask;
    *dest = (*dest & ~mask) | updated;

    return 0;
}

static void do_link_relocate_obj(struct obj *i, void **objtree, void **defns)
{
    list_foreach(objrlc, rlc, i->relocs)
        do_link_relocate_obj_reloc(i, rlc, objtree, defns);
}

static void do_link_relocate(struct obj_list *ol, void **objtree, void **defns)
{
    list_foreach(obj_list, Node, ol) {
        if (!Node->obj->rec_count) {
            debug(0, "Object has no records, skipping");
            continue;
        }

        do_link_relocate_obj(Node->obj, objtree, defns);
    }
}

static int do_link_process(struct link_state *s)
{
    void *objtree = NULL;   ///< tsearch-tree of `struct objmeta'
    void *defns   = NULL;   ///< tsearch tree of `struct defns'

    do_link_build_state(s, &objtree, &defns);
    do_link_relocate(s->objs, &objtree, &defns);

    while (objtree)
        tdelete(*(void**)objtree, &objtree, ptrcmp);
    while (defns)
        tdelete(*(void**)defns, &defns, (cmp*)strcmp);

    return 0;
}

int do_link_emit(struct link_state *s, struct obj *o)
{
    long rec_count = 0;
    // copy records
    struct objrec **ptr_objrec = &o->records, *front = NULL;
    int32_t addr = 0;
    list_foreach(obj_list, Node, s->objs) {
        struct obj *i = Node->obj;

        list_foreach(objrec, rec, i->records) {
            struct objrec *n = calloc(1, sizeof *n);

            n->addr = addr;
            n->size = rec->size;
            n->data = malloc(rec->size * sizeof *n->data);
            n->next = NULL;
            memcpy(n->data, rec->data, rec->size * sizeof *n->data);

            if (*ptr_objrec) (*ptr_objrec)->next = n;
            if (!front) front = n;
            *ptr_objrec = n;
            ptr_objrec = &n->next;

            addr += rec->size;
            rec_count++;
        }
    }

    o->records = front;

    o->rec_count = rec_count;
    o->sym_count = s->syms;
    o->rlc_count = s->rlcs;

    return 0;
}

static int do_link(struct link_state *s)
{
    int rc = -1;

    struct obj *o = s->relocated = calloc(1, sizeof *o);

    do_link_process(s);
    do_link_emit(s, o);

    return rc;
}

static int do_emit(struct link_state *s, FILE *out)
{
    int rc = -1;

    rc = obj_write(s->relocated, out);

    return rc;
}

int do_load_all(struct link_state *s, int count, char *names[count])
{
    int rc = 0;

    for (int i = 0; i < count; i++) {
        FILE *in = NULL;

        if (!strcmp(names[i], "-")) {
            in = stdin;
        } else {
            in = fopen(names[i], "rb");
            if (!in)
                fatal(PRINT_ERRNO, "Failed to open input file `%s'", names[i]);
        }

        rc = do_load(s, in);

        fclose(in);

        if (rc)
            return rc;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int rc = 0;

    struct link_state _s = {
        .addr = 0,
        .next_obj = &_s.objs,
    }, *s = &_s;

    char outfname[1024] = { 0 };
    FILE * volatile out = stdout;

    if ((rc = setjmp(errbuf))) {
        if (rc == DISPLAY_USAGE)
            usage(argv[0]);
        if (outfname[0] && out)
            // Technically there is a race condition here ; we would like to be
            // able to remove a file by a stream connected to it, but there is
            // apparently no portable way to do this.
            remove(outfname);
        return EXIT_FAILURE;
    }

    int ch;
    while ((ch = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (ch) {
            case 'o': out = fopen(strncpy(outfname, optarg, sizeof outfname), "wb"); break;
            case 'V': puts(version()); return EXIT_SUCCESS;
            case 'h':
                usage(argv[0]);
                return EXIT_FAILURE;
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc)
        fatal(DISPLAY_USAGE, "No input files specified on the command line");

    if (!out)
        fatal(PRINT_ERRNO, "Failed to open output file");

    rc = do_load_all(s, argc - optind, &argv[optind]);
    if (rc)
        fatal(PRINT_ERRNO, "Failed to load objects");
    do_link(s);
    do_emit(s, out);
    do_unload(s);
    list_foreach(objrec, rec, s->relocated->records) {
        free(rec->data);
        free(rec);
    }
    free(s->relocated);

    fclose(out);
    out = NULL;

    return rc;
}

/* vi: set ts=4 sw=4 et: */
