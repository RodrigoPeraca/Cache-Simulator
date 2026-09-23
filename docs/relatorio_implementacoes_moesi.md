# Relatorio das implementacoes atuais

## Organizacao

Foi criada a branch local `relatorio-implementacoes-moesi` para organizar este estado do projeto. Ela e uma branch local; a publicacao de um fork no GitHub depende de acesso ao repositorio remoto e autenticacao.

## Implementado

- `CacheState` com os estados `INVALID`, `SHARED`, `EXCLUSIVE`, `OWNED` e `MODIFIED`.
- `LinhaLRU` com `state`, `owner_core` e `dirty`.
- L1 privada por nucleo no LRU.
- L2 privada por nucleo no LRU.
- API do LRU recebe `core_id`.
- Configuracoes nomeadas por nivel e capacidade por nucleo.
- `Mockingjay` continua com sua API e comportamento originais.
- Teste automatizado em `tests/test_lru_private.c`.

## Como testar

No Windows com MinGW:

```powershell
mingw32-make test-lru
```

O teste verifica:

1. Primeiro acesso no core 0: miss na L1.
2. Segundo acesso no core 0: hit na L1.
3. Primeiro acesso no core 1 ao mesmo endereco: novo miss, comprovando L1 privada.
4. Segundo acesso no core 1: hit na L1.
5. O mesmo comportamento de miss/hit e repetido para as L2 privadas.
6. `core_id` invalido nao acessa uma cache valida.

A saida esperada inclui:

```text
PASS: L1 e L2 sao independentes por nucleo.
PASS: cada nova linha inicia no caminho EXCLUSIVE do modelo atual.
```

## Verificacao atual do MOESI

O teste imprime o estado interno das linhas. No modelo atual:

| Valor impresso | Estado      |
| -------------: | ----------- |
|              0 | `INVALID`   |
|              1 | `SHARED`    |
|              2 | `EXCLUSIVE` |
|              3 | `OWNED`     |
|              4 | `MODIFIED`  |

Para uma linha nova, a saida esperada e `estado=2`, `dono=0` ou `dono=1` e `dirty=0`.

Isso comprova apenas a infraestrutura de metadados e a inicializacao do estado `EXCLUSIVE`. Ainda nao e uma verificacao completa do protocolo MOESI.

## O que ainda nao esta implementado

- Barramento de coerencia e snooping.
- Operacoes `BusRd`, `BusRdX` e `BusUpgr`.
- Diferenciacao entre leitura e escrita no acesso do processador.
- Transicoes entre `EXCLUSIVE`, `SHARED`, `MODIFIED` e `OWNED`.
- Transferencia cache-to-cache e writeback.
- L3 unificada.
- Traces multicore com interleaving de acessos.

Portanto, neste momento nao e possivel validar uma transicao real `M -> O` ou `S -> M`; o caminho testavel e a criacao de uma linha nova como `EXCLUSIVE` e a preservacao dos metadados por nucleo.

## Proximo teste de coerencia

Depois da implementacao do barramento, o teste minimo devera executar dois cores sobre o mesmo bloco:

1. Core 0 le: `I -> E`.
2. Core 1 le: `E -> S` no core 0 e `I -> S` no core 1.
3. Core 0 escreve: `S -> M` no core 0 e `S -> I` no core 1.
4. Core 1 le: core 0 responde, `M -> O`, e core 1 entra em `S`.
5. A memoria so deve receber writeback quando a linha `O` for substituida ou invalidada.
