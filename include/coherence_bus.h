#ifndef COHERENCE_BUS_H
#define COHERENCE_BUS_H

#include <stdint.h>

#include "cache.h"

typedef int (*CoherenceSnoopReadHandler)(int snooper_core, uint32_t endereco);
typedef int (*CoherenceSnoopInvalidateHandler)(int snooper_core, uint32_t endereco);

void coherence_bus_init(CoherenceSnoopReadHandler snoop_read_handler,
						CoherenceSnoopInvalidateHandler snoop_invalidate_handler);
int coherence_bus_rd(int requester_core, uint32_t endereco);
int coherence_bus_rdx(int requester_core, uint32_t endereco);

#endif
