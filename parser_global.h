#ifndef PARSER_GLOBAL_H_
#define PARSER_GLOBAL_H_

#include "ops.h"
#include "parser.h"

struct parse_data {
    void *scanner;
    struct instruction_list *top;
    struct relocation_list {
        struct const_expr *ce;
        struct relocation_list *next;
    } *relocs;
    struct label_list {
        struct label *label;
        struct label_list *next;
    } *labels;
    uint32_t reladdr;
};

int tenor_parse(struct parse_data *);

#endif

