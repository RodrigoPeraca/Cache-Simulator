#include "../../include/lru.h"
#include "../../include/coherence_bus.h"
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

typedef struct {
    uint8_t valido;
    uint32_t tag;
    uint32_t idade;
} LinhaL3;

static LinhaLRU cache_lru[NUM_CORES][L1_NUM_SETS][L1_NUM_WAYS];
static LinhaLRU cache_L2_lru[NUM_CORES][L2_NUM_SETS][L2_NUM_WAYS];
static LinhaL3 cache_L3_lru[L3_NUM_SETS][L3_NUM_WAYS];

static int core_valido(int core_id) {
    return core_id >= 0 && core_id < NUM_CORES;
}

static int tipo_acesso_valido(CacheAccessType tipo_acesso) {
    return tipo_acesso == ACCESS_READ || tipo_acesso == ACCESS_WRITE;
}

static const char *nome_estado(CacheState state) {
    switch (state) {
        case INVALID: return "INVALID";
        case SHARED: return "SHARED";
        case EXCLUSIVE: return "EXCLUSIVE";
        case OWNED: return "OWNED";
        case MODIFIED: return "MODIFIED";
        default: return "UNKNOWN";
    }
}

static uint32_t obter_indice(uint32_t endereco) {
    uint32_t bloco = endereco / L1_BLOCK_SIZE_BYTES;
    return bloco % L1_NUM_SETS;
}

static uint32_t obter_tag(uint32_t endereco) {
    uint32_t bloco = endereco / L1_BLOCK_SIZE_BYTES;
    return bloco / L1_NUM_SETS;
}

static LinhaLRU *buscar_linha_l1(int core_id, uint32_t indice, uint32_t tag) {
    for (int via = 0; via < L1_NUM_WAYS; via++) {
        LinhaLRU *linha = &cache_lru[core_id][indice][via];
        if (linha->valido && linha->tag == tag) return linha;
    }
    return NULL;
}

static int snoop_leitura_l1(int snooper_core, uint32_t endereco) {
    if (!core_valido(snooper_core)) return 0;

    LinhaLRU *linha = buscar_linha_l1(snooper_core,
                                      obter_indice(endereco),
                                      obter_tag(endereco));
    if (linha == NULL) return 0;

    if (linha->state == EXCLUSIVE) {
        linha->state = SHARED;
        linha->owner_core = -1;
    } else if (linha->state == MODIFIED) {
        linha->state = OWNED;
    }

    return 1;
}

static int snoop_invalidar_l1(int snooper_core, uint32_t endereco) {
    if (!core_valido(snooper_core)) return 0;

    LinhaLRU *linha = buscar_linha_l1(snooper_core,
                                      obter_indice(endereco),
                                      obter_tag(endereco));
    if (linha == NULL) return 0;

    linha->valido = 0;
    linha->state = INVALID;
    linha->owner_core = -1;
    linha->dirty = 0;
    return 1;
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

static uint32_t obter_indice_L3(uint32_t endereco) {
    return (endereco / L3_BLOCK_SIZE_BYTES) % L3_NUM_SETS;
}

static uint32_t obter_tag_L3(uint32_t endereco) {
    return endereco / (L3_BLOCK_SIZE_BYTES * L3_NUM_SETS);
}

static void atualizar_idade_L3(uint32_t indice, int via_acessada) {
    uint32_t idade_antiga = cache_L3_lru[indice][via_acessada].idade;
    for (int i = 0; i < L3_NUM_WAYS; i++) {
        if (i == via_acessada) {
            cache_L3_lru[indice][i].idade = 0;
        } else if (cache_L3_lru[indice][i].valido &&
                   cache_L3_lru[indice][i].idade < idade_antiga) {
            cache_L3_lru[indice][i].idade++;
        }
    }
}

void inicializar_cache_lru(void) {
    coherence_bus_init(snoop_leitura_l1, snoop_invalidar_l1);

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

    for (int i = 0; i < L3_NUM_SETS; i++) {
        for (int j = 0; j < L3_NUM_WAYS; j++) {
            cache_L3_lru[i][j].valido = 0;
            cache_L3_lru[i][j].tag = 0;
            cache_L3_lru[i][j].idade = j;
        }
    }
    printf("[LRU] L1/L2 privadas e L3 compartilhada inicializadas.\n");
}

static void aplicar_tipo_acesso(LinhaLRU *linha, int core_id, CacheAccessType tipo_acesso) {
    if (tipo_acesso == ACCESS_WRITE) {
        linha->state = MODIFIED;
        linha->owner_core = core_id;
        linha->dirty = 1;
    }
}

int acessar_cache_lru(int core_id, uint32_t endereco, CacheAccessType tipo_acesso) {
    if (!core_valido(core_id) || !tipo_acesso_valido(tipo_acesso)) return 0;

    uint32_t indice = obter_indice(endereco);
    uint32_t tag = obter_tag(endereco);
    LinhaLRU *linha = buscar_linha_l1(core_id, indice, tag);

    if (linha != NULL) {
        int via = (int)(linha - &cache_lru[core_id][indice][0]);
        if (tipo_acesso == ACCESS_WRITE) {
            if (linha->state == SHARED || linha->state == OWNED) {
                coherence_bus_rdx(core_id, endereco);
            }
            if (linha->state == EXCLUSIVE || linha->state == SHARED ||
                linha->state == OWNED || linha->state == MODIFIED) {
                linha->state = MODIFIED;
                linha->owner_core = core_id;
                linha->dirty = 1;
            }
        }
        atualizar_lru(core_id, indice, via);
        return 1;
    }

    int via_substituir = -1;
    for (int via = 0; via < L1_NUM_WAYS; via++) {
        if (!cache_lru[core_id][indice][via].valido) {
            via_substituir = via;
            break;
        }
    }
    if (via_substituir == -1) {
        via_substituir = 0;
        for (int via = 1; via < L1_NUM_WAYS; via++) {
            if (cache_lru[core_id][indice][via].idade >
                cache_lru[core_id][indice][via_substituir].idade) {
                via_substituir = via;
            }
        }
    }

    int encontrou_copia_remota = 0;
    if (tipo_acesso == ACCESS_READ) {
        encontrou_copia_remota = coherence_bus_rd(core_id, endereco);
    } else {
        coherence_bus_rdx(core_id, endereco);
    }

    LinhaLRU *nova_linha = &cache_lru[core_id][indice][via_substituir];
    nova_linha->valido = 1;
    nova_linha->tag = tag;
    nova_linha->state = tipo_acesso == ACCESS_WRITE
        ? MODIFIED
        : (encontrou_copia_remota ? SHARED : EXCLUSIVE);
    nova_linha->owner_core = nova_linha->state == SHARED ? -1 : core_id;
    nova_linha->dirty = tipo_acesso == ACCESS_WRITE;
    atualizar_lru(core_id, indice, via_substituir);
    return 0;
}

CacheState consultar_estado_l1_lru(int core_id, uint32_t endereco) {
    if (!core_valido(core_id)) return INVALID;

    LinhaLRU *linha = buscar_linha_l1(core_id, obter_indice(endereco), obter_tag(endereco));
    return linha != NULL ? linha->state : INVALID;
}
int acessar_L2_lru(int core_id, uint32_t endereco, CacheAccessType tipo_acesso) {
    if (!core_valido(core_id) || !tipo_acesso_valido(tipo_acesso)) return 0;

    uint32_t indice = obter_indice_L2(endereco);
    uint32_t tag = obter_tag_L2(endereco);

    // Tenta o Hit
    for (int i = 0; i < L2_NUM_WAYS; i++) {
        if (cache_L2_lru[core_id][indice][i].valido && cache_L2_lru[core_id][indice][i].tag == tag) {
            aplicar_tipo_acesso(&cache_L2_lru[core_id][indice][i], core_id, tipo_acesso);
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
    aplicar_tipo_acesso(&cache_L2_lru[core_id][indice][via_substituir], core_id, tipo_acesso);
    atualizar_idade_L2(core_id, indice, via_substituir);
    return 0;
}

int acessar_L3_lru(uint32_t endereco, CacheAccessType tipo_acesso) {
    if (!tipo_acesso_valido(tipo_acesso)) return 0;
    uint32_t indice = obter_indice_L3(endereco);
    uint32_t tag = obter_tag_L3(endereco);

    for (int i = 0; i < L3_NUM_WAYS; i++) {
        if (cache_L3_lru[indice][i].valido && cache_L3_lru[indice][i].tag == tag) {
            atualizar_idade_L3(indice, i);
            return 1;
        }
    }

    int via_substituir = -1;
    for (int i = 0; i < L3_NUM_WAYS; i++) {
        if (!cache_L3_lru[indice][i].valido) {
            via_substituir = i;
            break;
        }
    }

    if (via_substituir == -1) {
        via_substituir = 0;
        for (int i = 1; i < L3_NUM_WAYS; i++) {
            if (cache_L3_lru[indice][i].idade > cache_L3_lru[indice][via_substituir].idade) {
                via_substituir = i;
            }
        }
    }

    cache_L3_lru[indice][via_substituir].valido = 1;
    cache_L3_lru[indice][via_substituir].tag = tag;
    atualizar_idade_L3(indice, via_substituir);
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
                          printf("Conjunto %d | Via %d | tag=0x%X | idade=%u | estado=%s | dono=%d | dirty=%d\n",
                              i, j, linha->tag, linha->idade, nome_estado(linha->state),
                           linha->owner_core, linha->dirty);
                }
            }
        }

        printf("\n[LRU] === CORE %d - L2 ===\n", core);
        for (int i = 0; i < L2_NUM_SETS; i++) {
            for (int j = 0; j < L2_NUM_WAYS; j++) {
                LinhaLRU *linha = &cache_L2_lru[core][i][j];
                if (linha->valido) {
                          printf("Conjunto %d | Via %d | tag=0x%X | idade=%u | estado=%s | dono=%d | dirty=%d\n",
                              i, j, linha->tag, linha->idade, nome_estado(linha->state),
                           linha->owner_core, linha->dirty);
                }
            }
        }
    }

    printf("\n[LRU] === L3 COMPARTILHADA ===\n");
    for (int i = 0; i < L3_NUM_SETS; i++) {
        for (int j = 0; j < L3_NUM_WAYS; j++) {
            LinhaL3 *linha = &cache_L3_lru[i][j];
            if (linha->valido) {
                printf("Conjunto %d | Via %d | tag=0x%X | idade=%u\n",
                       i, j, linha->tag, linha->idade);
            }
        }
    }
    printf("========================================\n");
}