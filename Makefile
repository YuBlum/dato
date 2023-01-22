BIN=dato

$(BIN): $(BIN).c
	gcc -Wall -Wextra -g -o $(BIN) $(BIN).c

run:
	./dato example.dato

valgrind: $(BIN).c
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind ./dato example.dato

gdb:
	gdb ./dato example.dato

clean:
	rm $(BIN)
