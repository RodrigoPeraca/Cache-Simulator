#include "../include/lru.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRACE_LINE 256
#define MAX_TOKENS 4

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    uint64_t misses;
    uint64_t reads;
    uint64_t writes;
} L1BenchmarkStats;

static int parse_core(const char *token, int *core_id) {
    char *end;
    errno = 0;
    long value = strtol(token, &end, 10);
    if (errno != 0 || *token == '\0' || *end != '\0' ||
        value < 0 || value >= NUM_CORES) {
        return 0;
    }
    *core_id = (int)value;
    return 1;
}

static int parse_address(const char *token, uint32_t *address) {
    char *end;
    errno = 0;
    unsigned long value = strtoul(token, &end, 16);
    if (errno != 0 || *token == '\0' || *end != '\0' || value > UINT32_MAX) {
        return 0;
    }
    *address = (uint32_t)value;
    return 1;
}

static int parse_operation(const char *token, CacheAccessType *operation) {
    if (token[1] != '\0') return 0;
    if (toupper((unsigned char)token[0]) == 'R') {
        *operation = ACCESS_READ;
        return 1;
    }
    if (toupper((unsigned char)token[0]) == 'W') {
        *operation = ACCESS_WRITE;
        return 1;
    }
    return 0;
}

static int parse_trace_line(char *line, int *core_id, uint32_t *address,
                            CacheAccessType *operation) {
    char *tokens[MAX_TOKENS];
    int token_count = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL && token_count < MAX_TOKENS) {
        if (token[0] == '#') break;
        tokens[token_count++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (token_count == 0) return 0;
    if (token_count > 3) return -1;

    *core_id = 0;
    *operation = ACCESS_READ;

    if (token_count == 1) {
        return parse_address(tokens[0], address) ? 1 : -1;
    }

    if (token_count == 2) {
        if (!parse_operation(tokens[0], operation) ||
            !parse_address(tokens[1], address)) {
            return -1;
        }
        return 1;
    }

    if (!parse_core(tokens[0], core_id) ||
        !parse_operation(tokens[1], operation) ||
        !parse_address(tokens[2], address)) {
        return -1;
    }
    return 1;
}

static double hit_rate(const L1BenchmarkStats *stats) {
    return stats->accesses == 0
        ? 0.0
        : (double)stats->hits * 100.0 / (double)stats->accesses;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo-trace>\n", argv[0]);
        fprintf(stderr, "Formatos: endereco | R/W endereco | core R/W endereco\n");
        return 2;
    }

    FILE *trace = fopen(argv[1], "r");
    if (trace == NULL) {
        perror(argv[1]);
        return 1;
    }

    L1BenchmarkStats per_core[NUM_CORES] = {{0}};
    L1BenchmarkStats total = {0};
    char line[MAX_TRACE_LINE];
    unsigned long line_number = 0;
    int parse_status;

    inicializar_cache_lru();

    while (fgets(line, sizeof(line), trace) != NULL) {
        int core_id;
        uint32_t address;
        CacheAccessType operation;
        line_number++;

        parse_status = parse_trace_line(line, &core_id, &address, &operation);
        if (parse_status == 0) continue;
        if (parse_status < 0) {
            fprintf(stderr,
                    "%s:%lu: registro invalido; esperado endereco, R/W endereco ou core R/W endereco\n",
                    argv[1], line_number);
            fclose(trace);
            return 1;
        }

        int hit = acessar_cache_lru(core_id, address, operation);
        L1BenchmarkStats *core_stats = &per_core[core_id];
        core_stats->accesses++;
        total.accesses++;
        if (operation == ACCESS_READ) {
            core_stats->reads++;
            total.reads++;
        } else {
            core_stats->writes++;
            total.writes++;
        }
        if (hit) {
            core_stats->hits++;
            total.hits++;
        } else {
            core_stats->misses++;
            total.misses++;
        }
    }

    if (ferror(trace)) {
        perror("Erro lendo trace");
        fclose(trace);
        return 1;
    }
    fclose(trace);

    printf("Benchmark L1 LRU: %s\n", argv[1]);
    printf("Escopo: somente L1; sem lookup de L2/L3.\n");
    for (int core_id = 0; core_id < NUM_CORES; core_id++) {
        const L1BenchmarkStats *stats = &per_core[core_id];
        printf("Core %d: acessos=%" PRIu64 " leituras=%" PRIu64
               " escritas=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
               " hit_rate=%.2f%%\n",
               core_id, stats->accesses, stats->reads, stats->writes,
               stats->hits, stats->misses, hit_rate(stats));
    }
    printf("Total: acessos=%" PRIu64 " leituras=%" PRIu64
           " escritas=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
           " hit_rate=%.2f%%\n",
           total.accesses, total.reads, total.writes,
           total.hits, total.misses, hit_rate(&total));
    return 0;
}
