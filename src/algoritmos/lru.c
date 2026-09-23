#include "../../include/lru.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t valido;
    uint32_t tag;
    uint32_t idade;
    CacheState state;
    int owner_core;
    uint8_t dirty;
} LinhaLRU;

static LinhaLRU cache_lru[NUM_CORES][L1_NUM_SETS][L1_NUM_WAYS];
static LinhaLRU cache_L2_lru[NUM_CORES][L2_NUM_SETS][L2_NUM_WAYS];

static int core_valido(int core_id) {
    return core_id >= 0 && core_id < NUM_CORES;
}

static uint32_t obter_indice(uint32_t endereco) {
    uint32_t bloco = endereco / L1_BLOCK_SIZE_BYTES;
    return bloco % L1_NUM_SETS;
}

static uint32_t obter_tag(uint32_t endereco) {
    uint32_t bloco = endereco / L1_BLOCK_SIZE_BYTES;
    return bloco / L1_NUM_SETS;
}

static void atualizar_lru(int core_id, uint32_t indice, int via_acessada) {
    uint32_t idade_antiga = cache_lru[core_id][indice][via_acessada].idade;

    for (int i = 0; i < L1_NUM_WAYS; i++) {
        if (i == via_acessada) {
            cache_lru[core_id][indice][i].idade = 0;
        } else if (cache_lru[core_id][indice][i].valido &&
                   cache_lru[core_id][indice][i].idade < idade_antiga) {
            cache_lru[core_id][indice][i].idade++;
        }
    }
}

// ==========================================
// FUNÇÕES AUXILIARES PARA L2 (Blocos de 64B)
// ==========================================
static uint32_t obter_indice_L2(uint32_t endereco) {
    return (endereco / L2_BLOCK_SIZE_BYTES) % L2_NUM_SETS;
}

static uint32_t obter_tag_L2(uint32_t endereco) {
    return endereco / (L2_BLOCK_SIZE_BYTES * L2_NUM_SETS);
}

static void atualizar_idade_L2(int core_id, uint32_t indice, int via_acessada) {
    uint32_t idade_antiga = cache_L2_lru[core_id][indice][via_acessada].idade;
    for (int i = 0; i < L2_NUM_WAYS; i++) {
        if (i == via_acessada) {
            cache_L2_lru[core_id][indice][i].idade = 0;
        } else if (cache_L2_lru[core_id][indice][i].valido && cache_L2_lru[core_id][indice][i].idade < idade_antiga) {
            cache_L2_lru[core_id][indice][i].idade++;
        }
    }
}

void inicializar_cache_lru(void) {
    for (int core = 0; core < NUM_CORES; core++) {
        for (int i = 0; i < L1_NUM_SETS; i++) {
            for (int j = 0; j < L1_NUM_WAYS; j++) {
                cache_lru[core][i][j].valido = 0;
                cache_lru[core][i][j].tag = 0;
                cache_lru[core][i][j].idade = j;
                cache_lru[core][i][j].state = INVALID;
                cache_lru[core][i][j].owner_core = -1;
                cache_lru[core][i][j].dirty = 0;
            }
        }

        for (int i = 0; i < L2_NUM_SETS; i++) {
            for (int j = 0; j < L2_NUM_WAYS; j++) {
                cache_L2_lru[core][i][j].valido = 0;
                cache_L2_lru[core][i][j].tag = 0;
                cache_L2_lru[core][i][j].idade = j;
                cache_L2_lru[core][i][j].state = INVALID;
                cache_L2_lru[core][i][j].owner_core = -1;
                cache_L2_lru[core][i][j].dirty = 0;
            }
        }
    }
    printf("[LRU] Caches L1 e L2 inicializadas.\n");
}

int acessar_cache_lru(int core_id, uint32_t endereco) {
    if (!core_valido(core_id)) return 0;

    uint32_t indice = obter_indice(endereco);
    uint32_t tag = obter_tag(endereco);

    for (int i = 0; i < L1_NUM_WAYS; i++) {
        if (cache_lru[core_id][indice][i].valido &&
            cache_lru[core_id][indice][i].tag == tag) {
            atualizar_lru(core_id, indice, i);
            return 1;
        }
    }

    int via_substituir = -1;

    for (int i = 0; i < L1_NUM_WAYS; i++) {
        if (!cache_lru[core_id][indice][i].valido) {
            via_substituir = i;
            break;
        }
    }

    if (via_substituir == -1) {
        via_substituir = 0;

        for (int i = 1; i < L1_NUM_WAYS; i++) {
            if (cache_lru[core_id][indice][i].idade > cache_lru[core_id][indice][via_substituir].idade) {
                via_substituir = i;
            }
        }
    }

    cache_lru[core_id][indice][via_substituir].valido = 1;
    cache_lru[core_id][indice][via_substituir].tag = tag;
    cache_lru[core_id][indice][via_substituir].state = EXCLUSIVE;
    cache_lru[core_id][indice][via_substituir].owner_core = core_id;
    cache_lru[core_id][indice][via_substituir].dirty = 0;

    atualizar_lru(core_id, indice, via_substituir);

    return 0;
}
int acessar_L2_lru(int core_id, uint32_t endereco) {
    if (!core_valido(core_id)) return 0;

    uint32_t indice = obter_indice_L2(endereco);
    uint32_t tag = obter_tag_L2(endereco);

    // Tenta o Hit
    for (int i = 0; i < L2_NUM_WAYS; i++) {
        if (cache_L2_lru[core_id][indice][i].valido && cache_L2_lru[core_id][indice][i].tag == tag) {
            atualizar_idade_L2(core_id, indice, i);
            return 1;
        }
    }

    // Miss: Busca quem expulsar
    int via_substituir = 0;
    for (int i = 0; i < L2_NUM_WAYS; i++) {
        if (!cache_L2_lru[core_id][indice][i].valido) {
            via_substituir = i;
            break;
        }
        if (cache_L2_lru[core_id][indice][i].idade > cache_L2_lru[core_id][indice][via_substituir].idade) {
            via_substituir = i;
        }
    }

    cache_L2_lru[core_id][indice][via_substituir].valido = 1;
    cache_L2_lru[core_id][indice][via_substituir].tag = tag;
    cache_L2_lru[core_id][indice][via_substituir].state = EXCLUSIVE;
    cache_L2_lru[core_id][indice][via_substituir].owner_core = core_id;
    cache_L2_lru[core_id][indice][via_substituir].dirty = 0;
    atualizar_idade_L2(core_id, indice, via_substituir);
    return 0;
}


void imprimir_estado_lru() {
    printf("\n[LRU] Estado atual das caches privadas:\n");

    for (int core = 0; core < NUM_CORES; core++) {
        printf("\n[LRU] === CORE %d - L1 ===\n", core);
        for (int i = 0; i < L1_NUM_SETS; i++) {
            for (int j = 0; j < L1_NUM_WAYS; j++) {
                LinhaLRU *linha = &cache_lru[core][i][j];
                if (linha->valido) {
                    printf("Conjunto %d | Via %d | tag=0x%X | idade=%u | estado=%d | dono=%d | dirty=%d\n",
                           i, j, linha->tag, linha->idade, linha->state,
                           linha->owner_core, linha->dirty);
                }
            }
        }

        printf("\n[LRU] === CORE %d - L2 ===\n", core);
        for (int i = 0; i < L2_NUM_SETS; i++) {
            for (int j = 0; j < L2_NUM_WAYS; j++) {
                LinhaLRU *linha = &cache_L2_lru[core][i][j];
                if (linha->valido) {
                    printf("Conjunto %d | Via %d | tag=0x%X | idade=%u | estado=%d | dono=%d | dirty=%d\n",
                           i, j, linha->tag, linha->idade, linha->state,
                           linha->owner_core, linha->dirty);
                }
            }
        }
    }
    printf("========================================\n");
}