#include "../include/coherence_bus.h"

static CoherenceSnoopReadHandler snoop_read_handler;
static CoherenceSnoopInvalidateHandler snoop_invalidate_handler;

void coherence_bus_init(CoherenceSnoopReadHandler read_handler,
                        CoherenceSnoopInvalidateHandler invalidate_handler) {
    snoop_read_handler = read_handler;
    snoop_invalidate_handler = invalidate_handler;
}

int coherence_bus_rd(int requester_core, uint32_t endereco) {
    if (requester_core < 0 || requester_core >= NUM_CORES ||
        snoop_read_handler == 0) {
        return 0;
    }

    int encontrou_copia = 0;
    for (int core = 0; core < NUM_CORES; core++) {
        if (core != requester_core && snoop_read_handler(core, endereco)) {
            encontrou_copia = 1;
        }
    }

    return encontrou_copia;
}

int coherence_bus_rdx(int requester_core, uint32_t endereco) {
    if (requester_core < 0 || requester_core >= NUM_CORES ||
        snoop_invalidate_handler == 0) {
        return 0;
    }

    int encontrou_copia = 0;
    for (int core = 0; core < NUM_CORES; core++) {
        if (core != requester_core &&
            snoop_invalidate_handler(core, endereco)) {
            encontrou_copia = 1;
        }
    }

    return encontrou_copia;
}
