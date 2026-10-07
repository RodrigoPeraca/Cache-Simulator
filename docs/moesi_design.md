# Resumo

Este documento descreve as mudanças necessárias para reimplementar o protocolo MOESI e preparar o simulador para multicore.

## Objetivos

- Introduzir os estados MOESI (Modified, Owned, Exclusive, Shared, Invalid).
- Permitir múltiplas caches L1 e L2 privadas, uma par L1/L2 por núcleo.
- Adicionar uma L3 unificada compartilhada entre os núcleos (implementada no LRU).
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

- Exemplo: `int acessar_cache_lru(int core_id, uint32_t endereco, CacheAccessType tipo_acesso);`

4. Substituir as instâncias estáticas de arrays de cache L1 por arrays por-core no `LRU`:

- `cache_lru[num_cores][L1_NUM_SETS][L1_NUM_WAYS]` ou alocar dinamicamente por `core`.

5. Fazer a `L2` por núcleo (cada core terá sua L2 privada) e adicionar uma `L3` unificada acima das L2s (concluído no caminho LRU).
6. `BusRd` e `BusRdX` implementados em `src/coherence_bus.c/h` como broadcasts síncronos para as L1s remotas; `BusUpgr` ainda não foi separado e falta a entrega de payload de dados:
   - Mensagens básicas: `BusRd`, `BusRdX`, `BusUpgr`, `Flush`/`BusWB`.
   - Cada cache, ao detectar miss/upgrade, envia mensagem ao barramento e o barramento notifica as caches.
7. Transições `Owned` básicas entre L1s implementadas: uma leitura remota causa `MODIFIED -> OWNED`, e a escrita de outro núcleo invalida o dono. A transferência dos dados sujos e writeback real ainda estão pendentes.
8. Atualizar a lógica de inserção/expulsão para considerar estados MOESI e necessidade de writeback (pendente; os bytes do bloco ainda não são modelados).
9. Atualizar `src/main.c` para suportar execução multicore:
   - Opção para fornecer X traces (ou um trace por core) e política de interleaving.
   - Inicializar caches por core e rodar o motor que alterna acessos entre cores.
10. Instrumentação: contadores por-core, contadores de mensagens no barramento, latências de writeback e transferências entre caches.
11. Teste unitário de transições L1 criado (`tests/test_moesi_l1.c`); traces multicore sintéticos e casos de concorrência/upgrade ainda estão pendentes.

## Mapa atual de structs, arrays e APIs

### `include/cache.h`

- `CacheStats`: contém apenas `hits`, `misses` e `acessos_totais`.
- Macros atuais definem L1/L2 por núcleo e L3 compartilhada:
  - L1: `L1_CAPACITY_PER_CORE_BYTES`, `L1_BLOCK_SIZE_BYTES`, `L1_NUM_WAYS` e `L1_NUM_SETS`.
  - L2: `L2_CAPACITY_PER_CORE_BYTES`, `L2_BLOCK_SIZE_BYTES`, `L2_NUM_WAYS` e `L2_NUM_SETS`.
- L3: `L3_CAPACITY_BYTES`, `L3_BLOCK_SIZE_BYTES`, `L3_NUM_WAYS` e `L3_NUM_SETS`.
- Deve passar a concentrar os tipos compartilhados: `CacheState`, metadados de uma linha, configuração por nível e estatísticas de coerência.

### `include/lru.h` e `src/algoritmos/lru.c`

- `LinhaLRU` é privada de `lru.c` e contém `valido`, `tag`, `idade`, `state`, `owner_core` e `dirty`.
- A L3 usa uma estrutura própria (`LinhaL3`) com `valido`, `tag` e `idade`; não tem proprietário MOESI.
- Os arrays atuais também são privados e globais ao processo:
  - `cache_lru[NUM_CORES][L1_NUM_SETS][L1_NUM_WAYS]`: uma L1 privada por núcleo.
  - `cache_L2_lru[NUM_CORES][L2_NUM_SETS][L2_NUM_WAYS]`: uma L2 privada por núcleo.
- APIs atuais:
  - `inicializar_cache_lru()` inicializa L1 e L2 de todos os núcleos.
  - `acessar_cache_lru(core_id, endereco, tipo_acesso)` acessa a L1 privada do núcleo informado.
  - `acessar_L2_lru(core_id, endereco, tipo_acesso)` acessa a L2 privada do núcleo informado.
  - `acessar_L3_lru(endereco, tipo_acesso)` acessa uma L3 única, compartilhada por todos os núcleos.
  - `imprimir_estado_lru()` imprime as estruturas internas.
- A adaptação para `core_id` e L1/L2 privadas está concluída no `LRU`, sem alterar o `Mockingjay`.
- A L3 é consultada após miss na L2; a L3 aplica LRU e é inicializada junto com as caches do LRU.
- `CacheAccessType` diferencia `ACCESS_READ` e `ACCESS_WRITE`. O LRU executa snoop síncrono entre L1s e aplica transições `I→E`, `E→S`, `S→M`, `M→O`, `O→I` e `E→M`. A escrita em `SHARED` ou `OWNED` invalida as cópias L1 dos demais núcleos.
- `BusRd` e `BusRdX` usam o módulo explícito síncrono. `BusUpgr` ainda usa `BusRdX`; as transações não transportam payload e a invalidação de uma linha dirty não faz writeback real. Coerência das L2/L3 também não está implementada.

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

- `main()` seleciona a política, abre um único trace e, no caminho LRU, envia misses em sequência por L1, L2 e L3.
- `CacheStats stats`, `stats_L2` e `stats_L3` são locais ao processamento de um trace.
- O parser do trace aceita `R endereco` e `W endereco`; endereço sem prefixo de operação continua sendo interpretado como leitura para manter compatibilidade com traces antigos.
- A futura versão multicore precisará separar estatísticas por núcleo e adicionar estatísticas da L3, mas isso deve ser feito somente quando a API do LRU estiver definida.

## Mapeamento de arquivos e locais a alterar

-- `[include/cache.h](include/cache.h)`: adicionar enum MOESI, tipos compartilhados, configuração de núcleos e parâmetros independentes para L1, L2 e L3.
-- `[include/lru.h](include/lru.h)`: ajustar assinaturas para `core_id` e declarar init per-core; aplicar mudanças inicialmente apenas ao `LRU`.
-- `[include/mockingjay.h](include/mockingjay.h)`: **manter inalterado por enquanto** (Mockingjay será tratado em etapa posterior).
-- `[src/algoritmos/lru.c](src/algoritmos/lru.c)`: substituir struct de linha por nova struct com `state`; adaptar funções `inicializar_*`, `acessar_*` e `imprimir_estado_*` para operar por core; integrar handlers que respondem a mensagens do barramento (callbacks ou API `coherence_bus_notify(..)`).
-- `[src/algoritmos/mockingjay.c](src/algoritmos/mockingjay.c)`: deixar inalterado nesta fase.
-- `src/main.c`: ainda falta parsing de número de cores/traces por core e interleaving multicore; o caminho em série L1/L2/L3 do LRU já está conectado.

- `src/coherence_bus.c` e `src/coherence_bus.h` (novo): implementar enfileiramento de mensagens e dispatch para caches.
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

## Teste da L3 compartilhada

Execute `mingw32-make test-l3` (ou `mingw32-make test` para todos os testes). O teste faz um acesso ao mesmo endereço no core 0 e depois no core 1: ambos têm miss em suas L1/L2 privadas, mas o segundo acesso encontra o bloco na L3 unificada.

Essa verificação cobre compartilhamento e lookup da L3. As transições de coerência L1 são verificadas separadamente por `mingw32-make test-moesi-l1`; execute `mingw32-make test` para rodar todas as suítes existentes.

As capacidades L3 usam valores padrão de 256 KiB, linha de 64 bytes e 16 vias; são ajustáveis pelos defines em `include/cache.h`. O `Mockingjay` continua sem L3 nesta etapa.

## Formato de acesso com leitura/escrita

Para indicar o tipo de operação no trace, use um registro por linha:

```text
R 0x4000
W 0x4000
R 0x4040
```

`R` significa leitura e `W` escrita (maiúsculas ou minúsculas). Traces antigos contendo somente endereços continuam sendo aceitos e cada registro é tratado como leitura. O trace `traces/moesi_rw_example.txt` serve como exemplo. A API LRU recebe `CacheAccessType` explicitamente; as APIs do `Mockingjay` continuam inalteradas.

As transições L1 são testadas em `tests/test_moesi_l1.c`. O teste cobre leitura exclusiva, compartilhamento após segunda leitura, invalidação em escrita, downgrade `MODIFIED→OWNED` quando outro núcleo lê e invalidação do dono `OWNED` quando o outro núcleo escreve. A implementação ainda não modela transferência de dados nem writeback, e não aplica snoop à L2/L3.
