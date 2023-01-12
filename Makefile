BIN=dato

dato: dato.c
	gcc -Wall -Wextra -o $(BIN) $(BIN).c

clean:
	rm $(BIN)
