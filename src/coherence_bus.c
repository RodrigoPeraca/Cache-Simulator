#include "../include/coherence_bus.h"

static CoherenceSnoopReadHandler snoop_read_handler;

void coherence_bus_init(CoherenceSnoopReadHandler handler) {
    snoop_read_handler = handler;
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
