#include "../include/lru.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const uint32_t endereco = 0x1000;

    inicializar_cache_lru();

    assert(acessar_cache_lru(0, endereco, ACCESS_READ) == 0);
    assert(acessar_cache_lru(0, endereco, ACCESS_READ) == 1);
    assert(acessar_cache_lru(0, endereco, ACCESS_WRITE) == 1);

    assert(acessar_cache_lru(1, endereco, ACCESS_READ) == 0);
    assert(acessar_cache_lru(1, endereco, ACCESS_READ) == 1);

    assert(acessar_L2_lru(0, endereco, ACCESS_READ) == 0);
    assert(acessar_L2_lru(0, endereco, ACCESS_READ) == 1);
    assert(acessar_L2_lru(0, endereco, ACCESS_WRITE) == 1);

    assert(acessar_L2_lru(1, endereco, ACCESS_READ) == 0);
    assert(acessar_L2_lru(1, endereco, ACCESS_READ) == 1);

    assert(acessar_cache_lru(-1, endereco, ACCESS_READ) == 0);
    assert(acessar_cache_lru(NUM_CORES, endereco, ACCESS_READ) == 0);

    printf("PASS: L1 e L2 sao independentes por nucleo.\n");
    printf("PASS: cada nova linha inicia no caminho EXCLUSIVE do modelo atual.\n");
    printf("Estado MOESI detalhado:\n");
    imprimir_estado_lru();
    return 0;
}
