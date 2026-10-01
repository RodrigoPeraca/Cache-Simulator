# Nome do executável
EXEC = simulador_cache

# Arquivos fonte
SRC = src/main.c src/algoritmos/lru.c src/algoritmos/mockingjay.c src/coherence_bus.c
TEST_LRU = tests/test_lru_private.exe
TEST_L3 = tests/test_l3_shared.exe
TEST_MOESI_L1 = tests/test_moesi_l1.exe

# Flags de compilação (mostra todos os avisos)
CFLAGS = -Wall -Iinclude

all:
	gcc $(CFLAGS) $(SRC) -o $(EXEC)

test-lru:
	gcc $(CFLAGS) tests/test_lru_private.c src/algoritmos/lru.c src/coherence_bus.c -o $(TEST_LRU)
	./$(TEST_LRU)

test-l3:
	gcc $(CFLAGS) tests/test_l3_shared.c src/algoritmos/lru.c src/coherence_bus.c -o $(TEST_L3)
	./$(TEST_L3)

test-moesi-l1:
	gcc $(CFLAGS) tests/test_moesi_l1.c src/algoritmos/lru.c src/coherence_bus.c -o $(TEST_MOESI_L1)
	./$(TEST_MOESI_L1)

test: test-lru test-l3 test-moesi-l1

clean:
	rm -f $(EXEC) $(TEST_LRU) $(TEST_L3) $(TEST_MOESI_L1)