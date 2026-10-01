#ifndef COHERENCE_BUS_H
#define COHERENCE_BUS_H

#include <stdint.h>

#include "cache.h"

typedef int (*CoherenceSnoopReadHandler)(int snooper_core, uint32_t endereco);

void coherence_bus_init(CoherenceSnoopReadHandler snoop_read_handler);
int coherence_bus_rd(int requester_core, uint32_t endereco);

#endif
