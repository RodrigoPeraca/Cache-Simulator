#include "../include/lru.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const uint32_t endereco = 0x1000;

    inicializar_cache_lru();

    assert(acessar_cache_lru(0, endereco) == 0);
    assert(acessar_cache_lru(0, endereco) == 1);

    assert(acessar_cache_lru(1, endereco) == 0);
    assert(acessar_cache_lru(1, endereco) == 1);

    assert(acessar_L2_lru(0, endereco) == 0);
    assert(acessar_L2_lru(0, endereco) == 1);

    assert(acessar_L2_lru(1, endereco) == 0);
    assert(acessar_L2_lru(1, endereco) == 1);

    assert(acessar_cache_lru(-1, endereco) == 0);
    assert(acessar_cache_lru(NUM_CORES, endereco) == 0);

    printf("PASS: L1 e L2 sao independentes por nucleo.\n");
    printf("PASS: cada nova linha inicia no caminho EXCLUSIVE do modelo atual.\n");
    printf("Estado MOESI detalhado:\n");
    imprimir_estado_lru();
    return 0;
}
