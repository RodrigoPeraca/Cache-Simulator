#include "../include/lru.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const uint32_t endereco_compartilhado = 0x8000;
    const uint32_t endereco_exclusivo = 0x9000;
    const uint32_t endereco_owned_owner_write = 0xA000;

    inicializar_cache_lru();

    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == INVALID);
    assert(acessar_cache_lru(0, endereco_compartilhado, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == EXCLUSIVE);

    assert(acessar_cache_lru(1, endereco_compartilhado, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == SHARED);
    assert(consultar_estado_l1_lru(1, endereco_compartilhado) == SHARED);

    assert(acessar_cache_lru(0, endereco_compartilhado, ACCESS_WRITE) == 1);
    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == MODIFIED);
    assert(consultar_estado_l1_lru(1, endereco_compartilhado) == INVALID);

    assert(acessar_cache_lru(1, endereco_compartilhado, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == OWNED);
    assert(consultar_estado_l1_lru(1, endereco_compartilhado) == SHARED);

    assert(acessar_cache_lru(1, endereco_compartilhado, ACCESS_WRITE) == 1);
    assert(consultar_estado_l1_lru(0, endereco_compartilhado) == INVALID);
    assert(consultar_estado_l1_lru(1, endereco_compartilhado) == MODIFIED);

    assert(acessar_cache_lru(0, endereco_exclusivo, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_exclusivo) == EXCLUSIVE);
    assert(acessar_cache_lru(0, endereco_exclusivo, ACCESS_WRITE) == 1);
    assert(consultar_estado_l1_lru(0, endereco_exclusivo) == MODIFIED);
    assert(consultar_estado_l1_lru(1, endereco_exclusivo) == INVALID);

    assert(acessar_cache_lru(0, endereco_owned_owner_write, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_owned_owner_write) == EXCLUSIVE);
    assert(acessar_cache_lru(0, endereco_owned_owner_write, ACCESS_WRITE) == 1);
    assert(consultar_estado_l1_lru(0, endereco_owned_owner_write) == MODIFIED);

    assert(acessar_cache_lru(1, endereco_owned_owner_write, ACCESS_READ) == 0);
    assert(consultar_estado_l1_lru(0, endereco_owned_owner_write) == OWNED);
    assert(consultar_estado_l1_lru(1, endereco_owned_owner_write) == SHARED);

    assert(acessar_cache_lru(0, endereco_owned_owner_write, ACCESS_WRITE) == 1);
    assert(consultar_estado_l1_lru(0, endereco_owned_owner_write) == MODIFIED);
    assert(consultar_estado_l1_lru(1, endereco_owned_owner_write) == INVALID);

    puts("PASS: transicoes MOESI L1 I->E, E->S, S->M, M->O, O->M, O->I e E->M.");
    return 0;
}
