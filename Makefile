# Nome do executável
EXEC = simulador_cache

# Arquivos fonte
SRC = src/main.c src/algoritmos/lru.c src/algoritmos/mockingjay.c
TEST_LRU = tests/test_lru_private.exe

# Flags de compilação (mostra todos os avisos)
CFLAGS = -Wall -Iinclude

all:
	gcc $(CFLAGS) $(SRC) -o $(EXEC)

test-lru:
	gcc $(CFLAGS) tests/test_lru_private.c src/algoritmos/lru.c -o $(TEST_LRU)
	./$(TEST_LRU)

clean:
	rm -f $(EXEC) $(TEST_LRU)