#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct token {
	enum {
		SEGMENT,
		TYPE,
		IDENTIFIER,
		SEMICOLON,
		OPEN_PAR,
		CLOSE_PAR,
		COMMA,
		OPERATOR,
		KEYWORD,
		INTEGER,
		UNKNOWN,
	}  type;
	char *str;
	int siz;
	struct token *nxt;
} token_t;

static char *token_type_str[] = {
	"SEGMENT",
	"TYPE",
	"IDENTIFIER",
	"SEMICOLON",
	"OPEN_PAR",
	"CLOSE_PAR",
	"COMMA",
	"OPERATOR",
	"KEYWORD",
	"INTEGER",
	"UNKNOWN",
};

static char *src;
static const char *const empty  = " \n";
static const char *const number = "0123456789";
static const char *const letter = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

token_t *
lex(token_t *prv) {

	while (strchr(empty, src[0])) {
		if (src[0] == '\0') return NULL;
		src++;
	}
	token_t *tkn = malloc(sizeof(token_t));
	tkn->str = src;

	if (strchr(letter, src[0])) {
		while (strchr(letter, src[0]) || strchr(number, src[0])) src++;
		tkn->siz = src - tkn->str;
		if (src[0] == ':') {
			src++;
			tkn->type = SEGMENT;
		} else if (strncmp(tkn->str, "b1", tkn->siz) == 0 ||
							 strncmp(tkn->str, "b2", tkn->siz) == 0 ||
							 strncmp(tkn->str, "b4", tkn->siz) == 0 ||
							 strncmp(tkn->str, "b8", tkn->siz) == 0 ||
							 strncmp(tkn->str, "ptr", tkn->siz) == 0) {
			tkn->type = TYPE;
		} else if (strncmp(tkn->str, "ret", tkn->siz) == 0 ||
						   strncmp(tkn->str, "end", tkn->siz) == 0) {
			tkn->type = KEYWORD;
		} else {
			tkn->type = IDENTIFIER;
		}
	} else if (strchr(number, src[0])) {
		while (strchr(number, src[0])) src++;
		tkn->siz = src - tkn->str;
		tkn->type = INTEGER;
	} else {
		src++;
		tkn->siz = src - tkn->str;
		if (strncmp(tkn->str, ";", tkn->siz) == 0) {
			tkn->type = SEMICOLON;
		} else if (strncmp(tkn->str, "(", tkn->siz) == 0) {
			tkn->type = OPEN_PAR;
		} else if (strncmp(tkn->str, ")", tkn->siz) == 0) {
			tkn->type = CLOSE_PAR;
		} else if (strncmp(tkn->str, ",", tkn->siz) == 0) {
			tkn->type = COMMA;
		} else if (strncmp(tkn->str, "+", tkn->siz) == 0 ||
					     strncmp(tkn->str, "-", tkn->siz) == 0 ||
					     strncmp(tkn->str, "*", tkn->siz) == 0 ||
					     strncmp(tkn->str, "/", tkn->siz) == 0 ||
					     strncmp(tkn->str, "=", tkn->siz) == 0) {
			tkn->type = OPERATOR;
		} else {
			tkn->type = UNKNOWN;
		}
	}
	
	if (prv) prv->nxt = tkn;

	return tkn;
}

int
main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "ERROR: file path not provided\n");
		fprintf(stderr, "Usage: %s <file-path>\n", argv[0]);
		exit(1);
	}
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		fprintf(stderr, "ERROR: could not open file %s: %s\n", argv[1], strerror(errno));
	}
	fseek(f, 0, SEEK_END);
	long f_siz = ftell(f);
	fseek(f, 0, SEEK_SET);
	src = malloc(f_siz);
	fread(src, f_siz, 1, f);
	fclose(f);

	token_t *tkn = NULL;
	do {
		tkn = lex(tkn);
		if (tkn) {
			printf("%s \t%.*s\n", token_type_str[tkn->type], tkn->siz, tkn->str);
		}
	} while (tkn);

	src -= f_siz;
	free(src);
	return 0;
}

