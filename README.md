# Simulador de Caches Inteligentes em RISC-V (LRU vs Mockingjay)

Projeto desenvolvido para a disciplina de Projeto Integrador IV, no semestre 2026/1, sob a orientação do Prof. Bruno S. Neves.

O objetivo deste projeto é implementar e avaliar algoritmos de substituição de blocos em cache, comparando o desempenho do algoritmo padrão LRU com o algoritmo baseado em heurística Mockingjay.

## Estrutura do Projeto (Fase 1 - Modelagem em C)

Nesta fase inicial (Semanas 1-4), o foco é a modelagem em software para validação do _Hit Rate_ lógico.

- `/include`: Arquivos de cabeçalho (`.h`) com as estruturas de dados e assinaturas.
- `/src/main.c`: Orquestrador do simulador que lê os arquivos de trace.
- `/src/algoritmos/lru.c`: Implementação da política _Least Recently Used_.
- `/src/algoritmos/mockingjay.c`: Implementação da política baseada em estimativa de tempo de reuso (Mockingjay).
- `/traces`: Arquivos de texto contendo sequências de endereços de memória para testes e validação.

## Como Compilar e Executar (Windows)

### Opção 1: Usando o Makefile (Recomendado)

O projeto conta com um `Makefile` para automatizar a compilação. No Windows, se você utiliza o MinGW, o comando correspondente é o `mingw32-make`.

1. **Verifique a instalação:**

   ```powershell
   mingw32-make --version
   Compilação: PowerShell
        -> mingw32-make
    Dica: Atalho para o comando make
    Para facilitar, você pode criar um apelido (alias) no   PowerShell para usar apenas make:
        -> Set-Alias -Name make -Value mingw32-make
        -> make

   ```

2. Opção 2: Compilação Direta via GCC
   Caso não utilize o Make, você pode compilar manualmente todos os arquivos fonte:

PowerShell

# gcc src/main.c src/algoritmos/lru.c src/algoritmos/mockingjay.c -o simulador_cache.exe

# simulador_cache.exe

### Teste da hierarquia privada do LRU

Para verificar a independência da L1 e da L2 entre os núcleos e visualizar os metadados MOESI atuais:

```powershell
mingw32-make test-lru
```

O teste confirma miss/hit independente para os cores 0 e 1 e imprime os metadados de cada linha.

Para verificar a L3 unificada compartilhada pelo caminho LRU:

```powershell
mingw32-make test-l3
```

O teste demonstra que o mesmo endereço pode causar miss nas L1/L2 privadas dos dois núcleos e hit na L3 compartilhada no segundo núcleo.

Para validar as transições MOESI implementadas nas L1s:

```powershell
mingw32-make test-moesi-l1
```

O teste verifica `I→E`, `E→S`, `S→M`, `M→O`, `O→I` e `E→M`. `mingw32-make test` executa todos os testes.

### Benchmark direto da L1

O runner standalone acessa diretamente `acessar_cache_lru()` e não consulta L2/L3. Para executar o trace intercalado de exemplo:

```powershell
mingw32-make bench-l1
```

Para escolher outro trace:

```powershell
mingw32-make bench-l1 TRACE=traces/circular_buffer_thrashing.txt
```

O runner aceita três formatos, um acesso por linha:

```text
0 R 0x8000
1 W 0x8000
R 0x8040
0x8080
```

O formato `core R/W endereço` simula acessos intercalados aos núcleos na ordem das linhas. Os formatos sem núcleo usam o core 0; endereço sem operação é uma leitura. O resultado mostra hits, misses e hit rate globais e por núcleo. Essas métricas descrevem o comportamento simulado da L1, não o tempo real do programa nem latência de memória.

### Formato dos traces para leitura e escrita

O caminho LRU aceita `R endereco` para leitura e `W endereco` para escrita, por exemplo:

```text
R 0x4000
W 0x4000
R 0x4040
```

Também são aceitos `r`/`w` minúsculos. Traces no formato antigo, com somente endereços, continuam sendo interpretados como leituras. `traces/moesi_rw_example.txt` contém uma sequência curta de exemplo. A coerência atual usa snoop síncrono entre as L1s do LRU. O modelo não representa os dados dos blocos e ainda não implementa writeback, latência do barramento ou snoop/coerência para L2 e L3.
