BIN=dato

$(BIN): $(BIN).c
	gcc -Wall -Wextra -o $(BIN) $(BIN).c -lm

clean:
	rm $(BIN)
