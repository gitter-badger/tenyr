#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdint.h>

struct sim_state;

#include "common.h"

typedef int map_init(struct sim_state *s, void *cookie, ...);
typedef int map_op(struct sim_state *s, void *cookie, int op, uint32_t addr, uint32_t *data);
typedef int map_cycle(struct sim_state *s, void *cookie);
typedef int map_fini(struct sim_state *s, void *cookie);

struct device {
    uint32_t bounds[2]; // lower and upper memory bounds, inclusive
    struct device_ops {
        map_init *init;
        map_op *op;
        map_cycle *cycle;
        map_fini *fini;
    } ops;
    void *cookie;
};

#endif

