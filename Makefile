BIN=dato

test: arena_allocator.c
	gcc -Wall -Wextra arena_allocator.c
	./a.out

$(BIN): $(BIN).c
	gcc -Wall -Wextra -o $(BIN) $(BIN).c

clean:
	rm $(BIN)
