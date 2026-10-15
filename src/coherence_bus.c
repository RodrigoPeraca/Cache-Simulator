#include "../include/coherence_bus.h"
#include <stdio.h>

static CoherenceSnoopReadHandler snoop_read_handler;
static CoherenceSnoopInvalidateHandler snoop_invalidate_handler;
static int verbose;

void coherence_bus_init(CoherenceSnoopReadHandler read_handler,
                        CoherenceSnoopInvalidateHandler invalidate_handler) {
    snoop_read_handler = read_handler;
    snoop_invalidate_handler = invalidate_handler;
}

void coherence_bus_set_verbose(int enabled) {
    verbose = enabled != 0;
}

int coherence_bus_is_verbose(void) {
    return verbose;
}

int coherence_bus_rd(int requester_core, uint32_t endereco) {
    if (requester_core < 0 || requester_core >= NUM_CORES ||
        snoop_read_handler == 0) {
        return 0;
    }

    if (verbose) {
        printf("  [BUS] BusRd requester=core%d addr=0x%08X\n",
               requester_core, (unsigned int)endereco);
    }

    int encontrou_copia = 0;
    int snoopers_com_copia = 0;
    for (int core = 0; core < NUM_CORES; core++) {
        if (core == requester_core) continue;

        int tem_copia = snoop_read_handler(core, endereco);
        if (verbose) {
            printf("  [BUS] SnoopRd core%d -> %s\n",
                   core, tem_copia ? "HIT" : "MISS");
        }
        if (tem_copia) {
            encontrou_copia = 1;
            snoopers_com_copia++;
        }
    }

    if (verbose) {
        printf("  [BUS] BusRd data_source=%s snoopers=%d\n",
               encontrou_copia ? "REMOTE_L1 (payload nao modelado)"
                       : "LOWER_LEVEL (lookup nao executado pelo runner L1)",
               snoopers_com_copia);
    }

    return encontrou_copia;
}

int coherence_bus_rdx(int requester_core, uint32_t endereco) {
    if (requester_core < 0 || requester_core >= NUM_CORES ||
        snoop_invalidate_handler == 0) {
        return 0;
    }

    if (verbose) {
        printf("  [BUS] BusRdX requester=core%d addr=0x%08X\n",
               requester_core, (unsigned int)endereco);
    }

    int encontrou_copia = 0;
    for (int core = 0; core < NUM_CORES; core++) {
        if (core == requester_core) continue;

        int invalidou_copia = snoop_invalidate_handler(core, endereco);
        if (verbose) {
            printf("  [BUS] SnoopRdX core%d -> %s\n",
                   core, invalidou_copia ? "INVALIDATED" : "NO_COPY");
        }
        if (invalidou_copia) encontrou_copia = 1;
    }

    if (verbose) {
        printf("  [BUS] BusRdX sharers_invalidated=%s\n",
               encontrou_copia ? "yes" : "no");
    }

    return encontrou_copia;
}
