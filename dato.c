#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define max(x, y) x > y ? x : y
#define min(x, y) x < y ? x : y

typedef struct token {
	enum {
		TKN_SEGMENT,
		TKN_TYPE,
		TKN_IDENTIFIER,
		TKN_SEMICOLON,
		TKN_LPARAN,
		TKN_RPARAN,
		TKN_COMMA,
		TKN_OPERATOR,
		TKN_KEYWORD,
		TKN_INTEGER,
		TKN_UNKNOWN,
	}  type;
	char *str;
	int siz;
	struct token *nxt;
} token_t;

typedef struct statement {
	token_t *tkn;
	token_t *htkn;
	struct statement *nxt;
} statement_t;

static char *token_type_str[] = {
	"TKN_SEGMENT",
	"TKN_TYPE",
	"TKN_IDENTIFIER",
	"TKN_SEMICOLON",
	"TKN_LPARAN",
	"TKN_RPARAN",
	"TKN_COMMA",
	"TKN_OPERATOR",
	"TKN_KEYWORD",
	"TKN_INTEGER",
	"TKN_UNKNOWN",
};

typedef struct ast {
	// only support signed integers for now
	enum {
		AST_VARDEF,
		AST_TYPE,
		AST_IDENTIFIER,
		AST_INTEGER,
		AST_ASSIGN,
		AST_PLUS,
		AST_RETURN,
		AST_VAR,
	} type;
	char *str;
	int siz;
	union {
		struct { struct ast *l; struct ast *r; } AST_VARDEF;
		struct { long long n; } AST_INTEGER;
		struct { struct ast *l; struct ast *r; } AST_ASSIGN;
		struct { struct ast *l; struct ast *r; } AST_PLUS;
		struct { char *str; int siz; } AST_IDENTIFIER;
		struct { char *str; int siz; } AST_TYPE;
		struct { struct ast *r; } AST_RETURN;
		// TODO: AST_VAR
	} leaves;
	statement_t *stat;
	statement_t *hstat;
} ast_t;

char *ast_type_str[] = {
	"AST_VARDEF",
	"AST_TYPE",
	"AST_IDENTIFIER",
	"AST_INTEGER",
	"AST_ASSIGN",
	"AST_PLUS",
	"AST_RETURN",
	"AST_VAR",
};

static char *src;
static const char *const empty  = " \n";
static const char *const number = "0123456789";
static const char *const letter = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static enum {
	SEG_LOGIC,
	SEG_DATA,
	SEG_SYSTEM,
	SEG_LAYOUT,
} segment = SEG_LOGIC;

void
print_token(token_t *tkn) {
	printf("%s %.*s", token_type_str[tkn->type], tkn->siz, tkn->str);
}


token_t *
file_to_tokens(token_t *prv) {
	while (strchr(empty, src[0])) {
		if (src[0] == '\0') return NULL;
		src++;
	}
	if (prv && prv->type == TKN_SEGMENT) return NULL;
	char *str = src;
	int siz, type;


	if (strchr(letter, src[0])) {
		while (strchr(letter, src[0]) || strchr(number, src[0])) src++;
		siz = src - str;
		if (src[0] == ':') {
			src++;
			type = TKN_SEGMENT;
		} else if (strncmp(str, "i1", 	max(siz, 2)) == 0 ||
							 strncmp(str, "i2", 	max(siz, 2)) == 0 ||
							 strncmp(str, "i4", 	max(siz, 2)) == 0 ||
							 strncmp(str, "i8", 	max(siz, 2)) == 0 ||
							 strncmp(str, "ptr", max(siz, 2)) == 0) {
			type = TKN_TYPE;
		} else if (strncmp(str, "ret", max(siz, 3)) == 0 ||
						   strncmp(str, "end", max(siz, 3)) == 0) {
			type = TKN_KEYWORD;
		} else {
			type = TKN_IDENTIFIER;
		}
	} else if (strchr(number, src[0])) {
		while (strchr(number, src[0])) src++;
		siz = src - str;
		type = TKN_INTEGER;
	} else {
		src++;
		siz = src - str;
		if (strncmp(str, ";", siz) == 0) {
			type = TKN_SEMICOLON;
		} else if (strncmp(str, "(", siz) == 0) {
			type = TKN_LPARAN;
		} else if (strncmp(str, ")", siz) == 0) {
			type = TKN_RPARAN;
		} else if (strncmp(str, ",", siz) == 0) {
			type = TKN_COMMA;
		} else if (strncmp(str, "+", siz) == 0 ||
					     strncmp(str, "-", siz) == 0 ||
					     strncmp(str, "*", siz) == 0 ||
					     strncmp(str, "/", siz) == 0 ||
					     strncmp(str, "=", siz) == 0) {
			type = TKN_OPERATOR;
		} else {
			type = TKN_UNKNOWN;
		}
	}
	if (type == TKN_SEMICOLON) return NULL;
	token_t *tkn = malloc(sizeof(token_t));
	tkn->siz = siz;
	tkn->str = str;
	tkn->type = type;
	
	if (prv) prv->nxt = tkn;

	return tkn;
}

statement_t *
tokens_to_statements(statement_t *prv) {
	if (src[0] == '\0') return NULL;
	statement_t *stat = malloc(sizeof(statement_t));
	stat->tkn = NULL;
	stat->htkn = NULL;

	do {
		stat->tkn = file_to_tokens(stat->tkn);
		if (!stat->htkn) stat->htkn = stat->tkn;
	} while (stat->tkn);

	if (stat->htkn == NULL) {
		free(stat);
		return NULL;
	}

	if (prv) prv->nxt = stat;

	return stat;
}

void
print_statement(statement_t *stat) {
	printf("{ ");
	stat->tkn = stat->htkn;
	while(stat->tkn) {
		print_token(stat->tkn);
		if (stat->tkn->nxt) {
			printf(", ");
		}
		stat->tkn = stat->tkn->nxt;
	}
	printf(" }\n");
}

void
change_segment(token_t *tkn) {
	if (tkn->type != TKN_SEGMENT) return;
	if (strncmp(tkn->str, "data", max(tkn->siz, 4)) == 0) {
		segment = SEG_DATA;
	} else if (strncmp(tkn->str, "logic", max(tkn->siz, 5)) == 0) {
		segment = SEG_LOGIC;
	} else if (strncmp(tkn->str, "system", max(tkn->siz, 6)) == 0) {
		segment = SEG_SYSTEM;
	} else if (strncmp(tkn->str, "layout", max(tkn->siz, 6)) == 0) {
		segment = SEG_LAYOUT;
	} else {
		// TODO: add position
		fprintf(stderr, "ERROR: '%.*s' is not a segment\n", tkn->siz, tkn->str);
	}
}

ast_t *
statements_to_ast() {
	ast_t *ast = malloc(sizeof(ast_t)), *r, *l;

	ast->stat = NULL;
	ast->hstat = NULL;
	int count;
	do {
		ast->stat = tokens_to_statements(ast->stat);
		if (!ast->hstat && ast->stat) ast->hstat = ast->stat;
		count++;
	} while(ast->stat);

	
	ast->stat = ast->hstat;
	token_t *tkn = ast->stat->htkn;

	while (ast->stat) {
		switch(segment) {
		case SEG_LOGIC:
			switch(tkn->type) {
			case TKN_SEGMENT: change_segment(tkn); break;
			default: 
				fprintf(stderr, "ERROR: '%.*s' is not handled in 'logic'\n", tkn->siz, tkn->str); 
				exit(1);
				break;
			}
			break;
		case SEG_DATA:
			switch(tkn->type) {
			case TKN_SEGMENT: change_segment(tkn); break;
			case TKN_TYPE: 
				if (!tkn->nxt) {
					fprintf(stderr, "ERROR: incomplete variable declaration\n");
					exit(1);
				}
				if (tkn->nxt->type != TKN_IDENTIFIER) {
					fprintf(stderr, "ERROR: %.*s is not a valid name for a variable\n", tkn->nxt->siz, tkn->nxt->str);
					exit(1);
				}
				r = malloc(sizeof(ast_t));
				l = malloc(sizeof(ast_t));

				l->type = AST_TYPE;
				l->leaves.AST_TYPE.str = tkn->str;
				l->leaves.AST_TYPE.siz = tkn->siz;

				r->type = AST_IDENTIFIER;
				r->leaves.AST_IDENTIFIER.str = tkn->nxt->str;
				r->leaves.AST_IDENTIFIER.siz = tkn->nxt->siz;

				ast->type = AST_VARDEF;
				ast->leaves.AST_VARDEF.l = l;
				ast->leaves.AST_VARDEF.r = r;
				tkn = tkn->nxt;
				break;
			default: 
				fprintf(stderr, "ERROR: '%.*s' is not handled in 'data'\n", tkn->siz, tkn->str); 
				exit(1);
				break;
			}
			break;
		case SEG_SYSTEM: assert(0 && "system segment not implemented"); break;
		case SEG_LAYOUT: assert(0 && "layout segment not implemented"); break;
		default: assert(0 && "unreachable");
		}
		tkn = tkn->nxt;
		if (!tkn) {
			ast->stat = ast->stat->nxt;
			tkn = ast->stat->htkn;
		}
	}

	return ast;
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


	ast_t *ast = statements_to_ast();

	while (ast->hstat) {
		while (ast->hstat->htkn){
			ast->hstat->tkn =  ast->hstat->htkn;
			ast->hstat->htkn = ast->hstat->htkn->nxt;
			free(ast->hstat->tkn);
		}
		ast->stat =  ast->hstat;
		ast->hstat = ast->hstat->nxt;
		free(ast->stat);
	}
	free(ast);

	src -= f_siz;
	free(src);
	return 0;
}

