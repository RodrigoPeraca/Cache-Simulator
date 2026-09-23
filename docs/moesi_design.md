# Resumo

Este documento descreve as mudanças necessárias para reimplementar o protocolo MOESI e preparar o simulador para multicore.

## Objetivos

- Introduzir os estados MOESI (Modified, Owned, Exclusive, Shared, Invalid).
- Permitir múltiplas caches L1 e L2 privadas, uma par L1/L2 por núcleo.
- Adicionar uma L3 unificada compartilhada entre os núcleos.
- Implementar um barramento de coerência que transporte mensagens entre caches.
- Preservar e adaptar políticas de substituição (LRU/Mockingjay).

## Alterações de alto nível

1. Adicionar enumeração de estados MOESI em `include/cache.h`.
2. Estender a struct de linha de cache — concluído no `LRU` — para incluir:
   - `state` (MOESI)
   - `owner_core` (id do dono quando aplicável)
   - `dirty` (opcional, para compatibilidade)
   - campo de validade/clock/heurística já existentes
3. Refatorar funções de acesso para receber `core_id` como primeiro argumento.
   - Exemplo: `int acessar_cache_lru(int core_id, uint32_t endereco);`
4. Substituir as instâncias estáticas de arrays de cache L1 por arrays por-core no `LRU`:

- `cache_lru[num_cores][L1_NUM_SETS][L1_NUM_WAYS]` ou alocar dinamicamente por `core`.

5. Fazer a `L2` por núcleo (cada core terá sua L2 privada) e adicionar uma `L3` unificada acima das L2s.
6. Implementar `src/coherence_bus.c/h` que modela o barramento e entrega mensagens (síncronas no simulador):
   - Mensagens básicas: `BusRd`, `BusRdX`, `BusUpgr`, `Flush`/`BusWB`.
   - Cada cache, ao detectar miss/upgrade, envia mensagem ao barramento e o barramento notifica as caches.
7. Implementar o tratamento do estado `Owned`:
   - Em `BusRd`, se uma cache possui linha em `Modified`, ela pode responder com dados e transitar para `Owned` (não escrever em memória imediatamente).
   - `Owned` conserva dados válidos e permite que outros caches estejam em `Shared` (memória pode estar desatualizada).
8. Atualizar a lógica de inserção/expulsão para considerar estados MOESI e necessidade de writeback.
9. Atualizar `src/main.c` para suportar execução multicore:
   - Opção para fornecer X traces (ou um trace por core) e política de interleaving.
   - Inicializar caches por core e rodar o motor que alterna acessos entre cores.
10. Instrumentação: contadores por-core, contadores de mensagens no barramento, latências de writeback e transferências entre caches.
11. Testes: criar traces multicore sintéticos e casos de coerência (read-after-write, write-after-read, upgrade races).

## Mapa atual de structs, arrays e APIs

### `include/cache.h`

- `CacheStats`: contém apenas `hits`, `misses` e `acessos_totais`.
- Macros atuais definem uma L1 e uma L2 global:
  - L1: `L1_CAPACITY_PER_CORE_BYTES`, `L1_BLOCK_SIZE_BYTES`, `L1_NUM_WAYS` e `L1_NUM_SETS`.
  - L2: `L2_CAPACITY_PER_CORE_BYTES`, `L2_BLOCK_SIZE_BYTES`, `L2_NUM_WAYS` e `L2_NUM_SETS`.
- Deve passar a concentrar os tipos compartilhados: `CacheState`, metadados de uma linha, configuração por nível e estatísticas de coerência.
- As constantes da L3 devem ser adicionadas separadamente; não reutilizar as macros da L2.

### `include/lru.h` e `src/algoritmos/lru.c`

- `LinhaLRU` é privada de `lru.c` e contém `valido`, `tag`, `idade`, `state`, `owner_core` e `dirty`.
- Os arrays atuais também são privados e globais ao processo:
  - `cache_lru[NUM_CORES][L1_NUM_SETS][L1_NUM_WAYS]`: uma L1 privada por núcleo.
  - `cache_L2_lru[NUM_CORES][L2_NUM_SETS][L2_NUM_WAYS]`: uma L2 privada por núcleo.
- APIs atuais:
  - `inicializar_cache_lru()` inicializa L1 e L2 de todos os núcleos.
  - `acessar_cache_lru(core_id, endereco)` acessa a L1 privada do núcleo informado.
  - `acessar_L2_lru(core_id, endereco)` acessa a L2 privada do núcleo informado.
  - `imprimir_estado_lru()` imprime as estruturas internas.
- A adaptação para `core_id` e L1/L2 privadas está concluída no `LRU`, sem alterar o `Mockingjay`.

### `include/mockingjay.h` e `src/algoritmos/mockingjay.c`

- `CacheLine` é privada de `mockingjay.c` e contém `tag`, `valid`, `ultimo_acesso` e `intervalo_previsto`.
- `cache[L1_NUM_SETS][L1_NUM_WAYS]`, `cache_L2[L2_NUM_SETS][L2_NUM_WAYS]` e `relogio_global` são símbolos globais.
- As APIs atuais permanecem inalteradas nesta etapa:
  - `inicializar_cache_mockingjay()`
  - `acessar_cache_mockingjay(endereco)`
  - `acessar_L2_mockingjay(endereco)`
  - `imprimir_estado_mockingjay(endereco)`
- Não adicionar `CacheState`, `core_id` ou suporte a L3 ao `Mockingjay` nesta fase.

### `src/main.c`

- `main()` seleciona a política, abre um único trace e envia cada endereço para L1 e, em caso de miss, para L2.
- `CacheStats stats` e `stats_L2` são locais ao processamento de um trace.
- A futura versão multicore precisará separar estatísticas por núcleo e adicionar estatísticas da L3, mas isso deve ser feito somente quando a API do LRU estiver definida.

## Mapeamento de arquivos e locais a alterar

-- `[include/cache.h](include/cache.h)`: adicionar enum MOESI, tipos compartilhados, configuração de núcleos e parâmetros independentes para L1, L2 e L3.
-- `[include/lru.h](include/lru.h)`: ajustar assinaturas para `core_id` e declarar init per-core; aplicar mudanças inicialmente apenas ao `LRU`.
-- `[include/mockingjay.h](include/mockingjay.h)`: **manter inalterado por enquanto** (Mockingjay será tratado em etapa posterior).
-- `[src/algoritmos/lru.c](src/algoritmos/lru.c)`: substituir struct de linha por nova struct com `state`; adaptar funções `inicializar_*`, `acessar_*` e `imprimir_estado_*` para operar por core; integrar handlers que respondem a mensagens do barramento (callbacks ou API `coherence_bus_notify(..)`).
-- `[src/algoritmos/mockingjay.c](src/algoritmos/mockingjay.c)`: deixar inalterado nesta fase.
-- `src/main.c`: adicionar parsing de número de cores / traces por core; inicialização de cada par L1/L2; acesso à L3 unificada; ciclo de execução multicore.

- `src/coherence_bus.c` e `src/coherence_bus.h` (novo): implementar enfileiramento de mensagens e dispatch para caches.
- `src/algoritmos/l3_lru.c` e `include/l3_lru.h` (novos, se a L3 usar LRU): implementar a L3 unificada sem misturar seus arrays com os da L2 privada.
- `Makefile` e `README.md`: adicionar build/test targets e instruções de uso multicore.

## Protocolos e mensagens (sugestão mínima)

- Mensagens do core -> barramento:
  - `BusRd(addr, core_id)`
  - `BusRdX(addr, core_id)` (requisição exclusiva para escrita)
  - `BusUpgr(addr, core_id)` (upgrade de Shared -> Exclusive/Modified sem transferir dados)
- Mensagens do barramento -> caches:
  - `SnoopRd(addr, requester_id)`
  - `SnoopRdX(addr, requester_id)`
  - `Flush(addr, source_id, dirty)` (envia dados ao requester / memória)
- Respostas entre caches (simuladas pelo barramento): transferência de dados direta entre caches quando presente (cache-to-cache transfer)

## Transições-chave (resumo)

- Read miss: requester envia `BusRd`.
  - Se nenhum outro detentor: fornecido da memória -> requester `Exclusive`.
  - Se outra cache tem `Modified`: ela envia `Flush` e muda para `Owned`; requester fica `Shared`.
  - Se outra cache tem `Owned`/`Shared`: owner fornece dados -> requester `Shared`.
- Write miss/upgrade: requester envia `BusRdX`/`BusUpgr`.
  - Outras caches invalidam suas linhas (`Invalid`).
  - Se alguém tinha `Modified`/`Owned`, envia `Flush` antes de invalidar.
  - Requester receberá `Modified`.

## Considerações de design e desempenho

- Inicialmente implementar barramento centralizado (síncrono) para simplicidade.
- Posteriormente avaliar diretório (menos broadcast) se número de núcleos crescer.
- Preferir alocação dinâmica de estruturas por-core para permitir testes com N variável.
- Manter overhead de estatísticas (contadores) configurável compile-time via macro.

## Próximos passos propostos (curto prazo)

1. Criar `src/coherence_bus.{c,h}` com API mínima para enviar e entregar mensagens.

As bases de MOESI e da hierarquia privada do `LRU` estão prontas. A próxima etapa é implementar o barramento de coerência, seguida pela L3 unificada. O `Mockingjay` continuará sem alterações.
