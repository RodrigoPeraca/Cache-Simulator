#include "../include/lru.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const uint32_t endereco_compartilhado = 0x4000;

    inicializar_cache_lru();

    assert(acessar_cache_lru(0, endereco_compartilhado, ACCESS_READ) == 0);
    assert(acessar_L2_lru(0, endereco_compartilhado, ACCESS_READ) == 0);
    assert(acessar_L3_lru(endereco_compartilhado, ACCESS_READ) == 0);

    assert(acessar_cache_lru(1, endereco_compartilhado, ACCESS_READ) == 0);
    assert(acessar_L2_lru(1, endereco_compartilhado, ACCESS_READ) == 0);
    assert(acessar_L3_lru(endereco_compartilhado, ACCESS_READ) == 1);

    assert(acessar_L3_lru(endereco_compartilhado + L3_BLOCK_SIZE_BYTES, ACCESS_READ) == 0);

    printf("PASS: o mesmo bloco causa miss na L1/L2 privada do segundo core e hit na L3 compartilhada.\n");
    printf("PASS: outro bloco mapeado pela API da L3 pode ser inserido separadamente.\n");
    return 0;
}
