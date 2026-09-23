#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h> // Biblioteca necessária para ler diretórios

#include "../include/cache.h"
#include "../include/lru.h"
#include "../include/mockingjay.h"

#define MAX_TRACES 50
#define MAX_FILENAME_LEN 256

int main() {
    char algoritmos[2][15] = {"LRU", "Mockingjay"};
    char traces[MAX_TRACES][MAX_FILENAME_LEN];
    int num_traces = 0;

    // Loop principal infinito até o usuário escolher sair (opção 0)
    while (1) {
        printf("\n==========================================\n");
        printf("   SIMULADOR DE CACHE - MENU PRINCIPAL\n");
        printf("==========================================\n");

        // 1. Escolher o Modo de Execução
        printf("\nEscolha o Modo de Execucao:\n");
        printf("1. Execucao Completa (Direto para o resultado)\n");
        printf("2. Passo a Passo (Validacao linha a linha)\n");
        printf("0. Sair do Programa\n");
        printf("Opcao: ");

        int modo_execucao;
        if (scanf("%d", &modo_execucao) != 1 || modo_execucao == 0) {
            printf("\nEncerrando o simulador...\n");
            break;
        }
        if (modo_execucao < 1 || modo_execucao > 2) {
            printf("\n[!] Opcao invalida. Tente novamente.\n");
            continue;
        }

        // 2. Escolher o Algoritmo
        printf("\n==========================================\n");
        printf("Escolha o Algoritmo:\n");
        printf("1. LRU\n");
        printf("2. Mockingjay\n");
        printf("0. Voltar\n");
        printf("Opcao: ");

        int op_algo;
        if (scanf("%d", &op_algo) != 1 || op_algo == 0) continue;
        if (op_algo < 1 || op_algo > 2) {
            printf("\n[!] Opcao invalida.\n");
            continue;
        }
        char *algoritmo_escolhido = algoritmos[op_algo - 1];

        // 3. Ler a pasta e listar os Traces
        DIR *d;
        struct dirent *dir;
        d = opendir("traces");
        num_traces = 0;

        if (d) {
            printf("\n==========================================\n");
            printf("Traces disponiveis na pasta 'traces/':\n");
            while ((dir = readdir(d)) != NULL) {
                // Ignora os ponteiros de pasta oculta do sistema "." e ".."
                if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                    strcpy(traces[num_traces], dir->d_name);
                    printf("%d. %s\n", num_traces + 1, traces[num_traces]);
                    num_traces++;
                    if (num_traces >= MAX_TRACES) break; // Limite de segurança
                }
            }
            closedir(d);
        } else {
            printf("\n[!] Erro: Nao foi possivel abrir a pasta 'traces'. Verifique se ela existe.\n");
            continue;
        }

        if (num_traces == 0) {
            printf("\n[!] Nenhum arquivo encontrado na pasta 'traces/'. Adicione alguns traces para testar.\n");
            continue;
        }

        // 4. Escolher o Trace
        printf("0. Voltar ao menu anterior\n");
        printf("Escolha o Trace que deseja rodar: ");
        int op_trace;
        if (scanf("%d", &op_trace) != 1 || op_trace == 0) continue;
        if (op_trace < 1 || op_trace > num_traces) {
            printf("\n[!] Opcao invalida.\n");
            continue;
        }

        char caminho_arquivo[MAX_FILENAME_LEN + 10];
        sprintf(caminho_arquivo, "traces/%s", traces[op_trace - 1]);

        // 5. Abrir arquivo e inicializar
        FILE *file = fopen(caminho_arquivo, "r");
        if (!file) {
            printf("\n[!] Erro ao abrir o arquivo: %s\n", caminho_arquivo);
            continue;
        }

        CacheStats stats = {0, 0, 0};
        CacheStats stats_L2 = {0, 0, 0}; // Para estatísticas da L2

        int is_lru = (op_algo == 1);
        int is_mockingjay = (op_algo == 2);

        if (is_lru) inicializar_cache_lru();
        else if (is_mockingjay) inicializar_cache_mockingjay();

        uint32_t endereco;
        
        // Limpa o buffer do teclado antes de entrar no loop de execução para não pular o primeiro 'Passo a Passo'
        while ((getchar()) != '\n'); 

        printf("\n--- Processando %s com politica %s ---\n", traces[op_trace - 1], algoritmo_escolhido);
        if (modo_execucao == 2) {
            printf("MODO PASSO A PASSO ATIVADO. Pressione [ENTER] para avancar ou 'q' para interromper.\n\n");
        }
        
        while (fscanf(file, "%x", &endereco) != EOF) {
            stats.acessos_totais++;
            int hit = 0;
            int hit_L2 = 0;
            int acessou_L2 = 0;

            // Chama a função correspondente
            if (is_lru) hit = acessar_cache_lru(0, endereco);
            else if (is_mockingjay) hit = acessar_cache_mockingjay(endereco);

            if (hit){ stats.hits++;
            }else {
                stats.misses++;
                    // 2. Se deu Miss na L1, o processador vai buscar na L2
                acessou_L2 = 1;
                stats_L2.acessos_totais++;                

                if (is_lru) hit_L2 = acessar_L2_lru(0, endereco);
                else if (is_mockingjay) hit_L2 = acessar_L2_mockingjay(endereco);

                if (hit_L2) {
                    stats_L2.hits++;
                    // Opcional/Avançado: Trazer o bloco da L2 para a L1 (forçando uma inserção/expulsão na L1)
                } else {
                    stats_L2.misses++;
                    // Deu Miss em tudo! Busca na RAM (Insere na L2 e depois insere na L1)
                }
            }
            
            // Lógica do Modo Passo a Passo
            if (modo_execucao == 2) {
                printf("==========================================\n");
                printf("Acesso %d: Endereco 0x%04X -> %s\n", stats.acessos_totais, endereco, hit ? "HIT" : "MISS");
                
                if (acessou_L2) {
                    printf("  L2: Endereco 0x%04X -> %s\n", endereco, hit_L2 ? "HIT" : "MISS");
                }
                // Opcional: Imprime o estado interno da cache para vocês conferirem
                if (is_lru) imprimir_estado_lru();
                else if (is_mockingjay) imprimir_estado_mockingjay(endereco);

                printf("Acao: [ENTER] proximo | [q] parar analise: ");
                
                char buffer[10];
                fgets(buffer, sizeof(buffer), stdin);
                if (buffer[0] == 'q' || buffer[0] == 'Q') {
                    printf("\n[!] Analise passo a passo interrompida pelo usuario.\n");
                    break;
                }
            }

        }
        fclose(file);

        // 6. Exibindo os Resultados Finais
        double hit_rate = (stats.acessos_totais > 0) ? ((double)stats.hits / stats.acessos_totais) * 100.0 : 0.0;
        double hr_L2 = (stats_L2.acessos_totais > 0) ? ((double)stats_L2.hits / stats_L2.acessos_totais) * 100.0 : 0.0;
        printf("\n=== RESULTADOS: %s ===\n", algoritmo_escolhido);
        printf("Arquivo: %s\n", traces[op_trace - 1]);

        printf("\n[CACHE L1 - DADOS]\n");
        printf("Acessos Totais: %d\n", stats.acessos_totais);
        printf("Hits: %d\n", stats.hits);
        printf("Misses: %d\n", stats.misses);
        printf("Hit Rate: %.2f%%\n", hit_rate);

        printf("\n[CACHE L2 - UNIFICADA]\n");
        printf("Acessos Totais: %d (Apenas os Misses da L1 chegam aqui)\n", stats_L2.acessos_totais);
        printf("Hits: %d | Misses: %d\n", stats_L2.hits, stats_L2.misses);
        printf("Hit Rate L2: %.2f%%\n", hr_L2);

        printf("\nPressione ENTER para voltar ao menu principal...");
        if (modo_execucao == 1) {
            while ((getchar()) != '\n'); // Limpa buffer se veio do modo completo
        }
        getchar(); // Aguarda o usuário
    }

    return 0;
}