#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>

typedef struct {
    int hits;
    int misses;
    int acessos_totais;
} CacheStats;

typedef enum {
    INVALID,
    SHARED,
    EXCLUSIVE,
    OWNED,
    MODIFIED
} CacheState;

#define NUM_CORES 2

// --- CONFIGURAÇÕES DA CACHE L1 (Baseado na Especificação) ---
#define L1_CAPACITY_PER_CORE_BYTES 4096 // 4 KB por núcleo
#define L1_BLOCK_SIZE_BYTES        32   // 32 Bytes
#define L1_NUM_WAYS                2    // Associatividade: 2-vias
// O compilador calcula os conjuntos automaticamente para nós!
#define L1_NUM_SETS                 (L1_CAPACITY_PER_CORE_BYTES / (L1_BLOCK_SIZE_BYTES * L1_NUM_WAYS))

// --- CONFIGURAÇÕES DA CACHE L2 (Privada por núcleo) ---
#define L2_CAPACITY_PER_CORE_BYTES 32768 // 32 KB por núcleo
#define L2_BLOCK_SIZE_BYTES        64    // 64 Bytes
#define L2_NUM_WAYS                8     // 8-vias
#define L2_NUM_SETS                (L2_CAPACITY_PER_CORE_BYTES / (L2_BLOCK_SIZE_BYTES * L2_NUM_WAYS))

#endif