BIN=dato

run: dato
	./$(BIN)

dato: dato.c
	gcc -Wall -Wextra -o $(BIN) $(BIN).c

clean:
	rm $(BIN)
