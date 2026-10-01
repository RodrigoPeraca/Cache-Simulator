# Fluxo de coerência MOESI nas L1

Este documento explica o comportamento de coerência implementado até agora no caminho `LRU`. O escopo atual é restrito às caches L1 privadas dos núcleos. A L2 privada e a L3 compartilhada continuam funcionando como níveis de lookup, mas não participam das transições MOESI.

## Estruturas envolvidas

Cada linha L1 do LRU mantém:

- `valido`: indica se a entrada contém uma linha utilizável.
- `tag`: identifica o bloco armazenado.
- `idade`: metadado da política de substituição LRU.
- `state`: estado MOESI da cópia local.
- `owner_core`: núcleo associado à cópia, ou `-1` quando o estado é compartilhado sem proprietário exclusivo.
- `dirty`: indica que a cópia local foi modificada.

A API de acesso recebe núcleo, endereço e operação:

```c
int acessar_cache_lru(int core_id, uint32_t endereco, CacheAccessType tipo_acesso);
```

`ACCESS_READ` representa leitura e `ACCESS_WRITE` representa escrita. O retorno continua sendo `1` para hit na L1 e `0` para miss, independentemente da transição de estado aplicada.

## Como funciona o snoop atual

Quando um núcleo acessa uma linha, a implementação calcula índice e tag e procura a linha em sua própria L1.

- Em hit, uma leitura mantém o estado atual; uma escrita pode atualizar o estado e invalidar cópias remotas quando necessário.
- Em miss de leitura, o código procura a mesma linha nas L1s dos outros núcleos. Se encontrar cópia, atualiza o estado remoto e instala a nova cópia em `SHARED`; se não encontrar, instala em `EXCLUSIVE`.
- Em miss de escrita, invalida cópias encontradas nas L1s remotas e instala a linha do solicitante em `MODIFIED`.

Em miss de leitura, `src/algoritmos/lru.c` chama `coherence_bus_rd()` em `src/coherence_bus.c`. O barramento faz broadcast síncrono para todas as L1s, exceto a solicitante, e chama o handler de snoop registrado pelo LRU. O handler procura a linha e atualiza `EXCLUSIVE→SHARED` ou `MODIFIED→OWNED`. O retorno informa se alguma L1 remota tinha uma cópia; o solicitante usa essa resposta para instalar `SHARED` ou `EXCLUSIVE`.

Esse `BusRd` é uma transação funcional simplificada: não há fila, arbitragem, latência nem payload de dados. Ele não é enviado em hit de leitura na L1.

## Transições presentes

| Operação                                            | Estado anterior     | Resultado no núcleo solicitante | Efeito nas outras L1s                        |
| --------------------------------------------------- | ------------------- | ------------------------------- | -------------------------------------------- |
| Leitura, linha ausente e sem cópias                 | `INVALID`           | `EXCLUSIVE`                     | Nenhum                                       |
| Leitura, linha ausente e outra cópia em `EXCLUSIVE` | `INVALID`           | `SHARED`                        | A cópia existente passa `EXCLUSIVE → SHARED` |
| Leitura, linha ausente e outra cópia em `MODIFIED`  | `INVALID`           | `SHARED`                        | A cópia modificada passa `MODIFIED → OWNED`  |
| Escrita em linha própria `EXCLUSIVE`                | `EXCLUSIVE`         | `MODIFIED`                      | Nenhum                                       |
| Escrita em linha própria `SHARED`                   | `SHARED`            | `MODIFIED`                      | As outras cópias são invalidadas             |
| Escrita em linha própria `OWNED`                    | `OWNED`             | `MODIFIED`                      | As outras cópias são invalidadas             |
| Escrita em linha própria `MODIFIED`                 | `MODIFIED`          | `MODIFIED`                      | Nenhum                                       |
| Escrita com miss na L1                              | `INVALID`           | `MODIFIED`                      | Cópias remotas encontradas são invalidadas   |
| Escrita remota em uma linha compartilhada ou owned  | `SHARED` ou `OWNED` | `MODIFIED` no solicitante       | A cópia remota passa a inválida              |

`INVALID` também é o resultado de `consultar_estado_l1_lru()` quando não há linha válida para o endereço ou quando o identificador de núcleo é inválido.

## Exemplo passo a passo

Considere o mesmo endereço acessado pelos núcleos 0 e 1:

1. Core 0 lê: não há cópias; instala `E` (`INVALID → EXCLUSIVE`).
2. Core 1 lê: encontra a cópia do core 0; core 0 passa `E → S`, e core 1 recebe `S`.
3. Core 0 escreve: invalida a cópia S do core 1; core 0 passa `S → M`.
4. Core 1 lê: encontra a cópia M do core 0; core 0 passa `M → O`, e core 1 recebe `S`.
5. Core 1 escreve: invalida a cópia O do core 0; core 1 passa `S → M`.

Os estados são verificáveis com `consultar_estado_l1_lru(core_id, endereco)`.

## Como testar

Execute o teste focado:

```powershell
mingw32-make test-moesi-l1
```

O caso está implementado em `tests/test_moesi_l1.c` e valida `I→E`, `E→S`, `S→M`, `M→O`, `O→I` e `E→M`. Para executar também os testes de hierarquia privada e L3:

```powershell
mingw32-make test
```

## O que este modelo ainda não simula

As transições descrevem estados e validade das cópias, não movimentação real de dados. As estruturas não armazenam bytes de cada linha. Portanto:

- `BusRd` já é um broadcast síncrono explícito. `BusRdX` e `BusUpgr` ainda não são transações do barramento; as escritas invalidam cópias diretamente pelo LRU.
- O `dirty` é metadado; não existe writeback de conteúdo modificado para L2, L3 ou memória.
- Ao invalidar uma linha remota `MODIFIED` ou `OWNED`, não há transferência explícita dos dados sujos antes da invalidação.
- A L2 e a L3 não são snoopadas nem mantêm estados MOESI coerentes com as L1s.
- O programa principal atual processa um trace por vez no core 0. Os testes chamam as APIs diretamente para simular acessos alternados de vários núcleos.
- O modelo de leitura que instala `SHARED` depois de encontrar qualquer cópia remota é simplificado; as regras detalhadas para combinações com `OWNED` e múltiplos sharers precisarão ser formalizadas ao introduzir as mensagens e respostas do barramento.

Assim, este é um primeiro modelo funcional de transições de estado L1 para testes, não uma implementação completa de coerência de hardware.
