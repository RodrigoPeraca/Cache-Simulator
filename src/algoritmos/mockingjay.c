#include "../../include/mockingjay.h"
#include <stdio.h>

#define MAX_INTERVALO 999999 // Representa o "desconhecido" para blocos novos

typedef struct {
    uint32_t tag;
    int valid;
    int ultimo_acesso;      // Tempo do relógio no último acesso
    int intervalo_previsto; // O "padrão" aprendido pela heurística
} CacheLine;

CacheLine cache[L1_NUM_SETS][L1_NUM_WAYS];
CacheLine cache_L2[L2_NUM_SETS][L2_NUM_WAYS]; // Para simular a cache L2 deste algoritmo
int relogio_global = 0;

void inicializar_cache_mockingjay() {
    for (int i = 0; i < L1_NUM_SETS; i++) {
        for (int j = 0; j < L1_NUM_WAYS; j++) {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].ultimo_acesso = 0;
            cache[i][j].intervalo_previsto = MAX_INTERVALO; 
        }
    }
    relogio_global = 0;
    printf("[Mockingjay] Cache inicializada.\n");
}

int calcular_conjunto(uint32_t endereco) {
    return (endereco / L1_BLOCK_SIZE_BYTES) % L1_NUM_SETS;
}

uint32_t calcular_tag(uint32_t endereco) {
    return endereco / (L1_BLOCK_SIZE_BYTES * L1_NUM_SETS);
}

int acessar_cache_mockingjay(uint32_t endereco) {
    int conjunto = calcular_conjunto(endereco);
    uint32_t tag = calcular_tag(endereco);
    
    relogio_global++; // O tempo avança a cada instrução lida

    // 1. Verificar HIT
    for (int v = 0; v < L1_NUM_WAYS; v++) {
        if (cache[conjunto][v].valid && cache[conjunto][v].tag == tag) {
            // FASE DE APRENDIZADO (Warm-up)
            int tempo_passado = relogio_global - cache[conjunto][v].ultimo_acesso;
            
            // O algoritmo "aprende" o intervalo de reuso deste bloco!
            cache[conjunto][v].intervalo_previsto = tempo_passado; 
            cache[conjunto][v].ultimo_acesso = relogio_global;
            
            return 1; // Hit
        }
    }

    // 2. Verificar MISS (Decisão de Substituição)
    int via_substituir = -1;
    int max_etr = -1; // Maior Tempo Estimado de Reuso (vítima)

    for (int v = 0; v < L1_NUM_WAYS; v++) {
        // Se a via está vazia, ocupa ela sem expulsar ninguém
        if (!cache[conjunto][v].valid) {
            via_substituir = v;
            break; 
        }
        
        // CÁLCULO DA HEURÍSTICA: Quando este bloco deve ser pedido de novo?
        // ETR = (Tempo do Último Acesso + O intervalo que ele costuma levar) - Tempo Atual
        int tempo_estimado_reuso = (cache[conjunto][v].ultimo_acesso + cache[conjunto][v].intervalo_previsto) - relogio_global;
        
        // Se a heurística der negativo, significa que ele já devia ter sido chamado e não foi.
        // Assumimos que a predição errou e ele vai demorar muito.
        if (tempo_estimado_reuso < 0) tempo_estimado_reuso = MAX_INTERVALO;

        // Procuramos o bloco com o maior tempo de reuso (o que vai demorar mais)
        if (tempo_estimado_reuso > max_etr) {
            max_etr = tempo_estimado_reuso;
            via_substituir = v;
        }
    }

    // 3. Substituir o bloco
    cache[conjunto][via_substituir].tag = tag;
    cache[conjunto][via_substituir].valid = 1;
    cache[conjunto][via_substituir].ultimo_acesso = relogio_global;
    
    // Como acabou de entrar e não tem histórico, colocamos o intervalo como desconhecido
    cache[conjunto][via_substituir].intervalo_previsto = MAX_INTERVALO; 

    return 0; // Miss
}

int acessar_L2_mockingjay(uint32_t endereco) {
    uint32_t conjunto = (endereco / L2_BLOCK_SIZE_BYTES) % L2_NUM_SETS;
    uint32_t tag = endereco / (L2_BLOCK_SIZE_BYTES * L2_NUM_SETS);

    // 1. Tentar o Hit na L2
    for (int v = 0; v < L2_NUM_WAYS; v++) {
        if (cache_L2[conjunto][v].valid && cache_L2[conjunto][v].tag == tag) {
            // Usa o mesmo relógio da CPU para calcular o intervalo da L2
            int tempo_passado = relogio_global - cache_L2[conjunto][v].ultimo_acesso;
            cache_L2[conjunto][v].intervalo_previsto = tempo_passado; 
            cache_L2[conjunto][v].ultimo_acesso = relogio_global;
            return 1; // Hit
        }
    }

    // 2. Miss na L2: Decidir expulsão pela Heurística
    int via_substituir = 0;
    int max_etr = -1;

    for (int v = 0; v < L2_NUM_WAYS; v++) {
        if (!cache_L2[conjunto][v].valid) {
            via_substituir = v;
            break; 
        }
        int etr = (cache_L2[conjunto][v].ultimo_acesso + cache_L2[conjunto][v].intervalo_previsto) - relogio_global;
        if (etr < 0) etr = MAX_INTERVALO;

        if (etr > max_etr) {
            max_etr = etr;
            via_substituir = v;
        }
    }

    // 3. Substituir
    cache_L2[conjunto][via_substituir].tag = tag;
    cache_L2[conjunto][via_substituir].valid = 1;
    cache_L2[conjunto][via_substituir].ultimo_acesso = relogio_global;
    cache_L2[conjunto][via_substituir].intervalo_previsto = MAX_INTERVALO; 
    return 0; // Miss
}

void imprimir_estado_mockingjay(uint32_t endereco) {
   printf("\n[MOCKINGJAY] === ESTADO ATUAL DA CACHE L1 ===\n");
    for (int i = 0; i < L1_NUM_SETS; i++) {
        int conjunto_tem_dado = 0;
        for (int j = 0; j < L1_NUM_WAYS; j++) {
            if (cache[i][j].valid) { conjunto_tem_dado = 1; break; }
        }
        if (!conjunto_tem_dado) continue;

        printf("Conjunto %d: ", i);
        for (int j = 0; j < L1_NUM_WAYS; j++) {
            if (cache[i][j].valid) {
                printf("[Tag: 0x%X | IntPreditivo: %d]", cache[i][j].tag, cache[i][j].intervalo_previsto);
            } else {
                printf("[Vazio]");
            }
            if (j < L1_NUM_WAYS - 1) printf(" | ");
        }
        printf("\n");
    }

    printf("\n[MOCKINGJAY] === ESTADO ATUAL DA CACHE L2 ===\n");
    for (int i = 0; i < L2_NUM_SETS; i++) {
        int conjunto_tem_dado = 0;
        for (int j = 0; j < L2_NUM_WAYS; j++) {
            if (cache_L2[i][j].valid) { conjunto_tem_dado = 1; break; }
        }
        if (!conjunto_tem_dado) continue;

        printf("Conjunto %d: ", i);
        for (int j = 0; j < L2_NUM_WAYS; j++) {
            if (cache_L2[i][j].valid) {
                printf("[Tag: 0x%X | IntPreditivo: %d]", cache_L2[i][j].tag, cache_L2[i][j].intervalo_previsto);
            } else {
                printf("[Vazio]");
            }
            if (j < L2_NUM_WAYS - 1) printf(" | ");
        }
        printf("\n");
    }
    printf("========================================\n");
}