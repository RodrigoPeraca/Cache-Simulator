// include/lru.h
#ifndef LRU_H
#define LRU_H
#include "cache.h"
#include <stdint.h>

void inicializar_cache_lru(void);
int acessar_cache_lru(int core_id, uint32_t endereco);
int acessar_L2_lru(int core_id, uint32_t endereco); // Retorna 1 para Hit, 0 para Miss
void imprimir_estado_lru(void); // Útil para validação linha a linha

#endif