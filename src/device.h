#ifndef DEVICES_H_
#define DEVICES_H_

#include <stdint.h>

#include "common.h"
#include "sim.h"

typedef int map_init(struct sim_state *s, void *cookie, ...);
typedef int map_op(struct sim_state *s, void *cookie, int op, uint32_t addr, uint32_t *data);
typedef int map_cycle(struct sim_state *s, void *cookie);
typedef int map_fini(struct sim_state *s, void *cookie);

struct device {
    uint32_t bounds[2]; // lower and upper memory bounds, inclusive
    map_init *init;
    map_op *op;
    map_cycle *cycle;
    map_fini *fini;
    void *cookie;
};

#endif

