BIN=dato

alloc:
	gcc -Wall -Wextra -g -o alloc arena_allocator.c
	./alloc

$(BIN): $(BIN).c
	gcc -Wall -Wextra -o $(BIN) $(BIN).c -lm

clean:
	rm $(BIN)
