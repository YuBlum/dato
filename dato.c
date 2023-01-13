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
		AST_PROGRAM,
		AST_VARDEF,
		AST_TYPE,
		AST_IDENTIFIER,
		AST_INTEGER,
		AST_SECTION,
		AST_ASSIGN,
		AST_PLUS,
		AST_RETURN,
		AST_VAR,
	} type;
	union {
		struct {
			char *str;
			int siz;
		};
	} value;
	struct ast *branches;
	struct ast *top_branch;
	unsigned int branches_siz;
	unsigned int branches_cap;
	statement_t *stat;
	statement_t *hstat;
} ast_t;

char *ast_type_str[] = {
	"PROGRAM",
	"VARDEF",
	"TYPE",
	"IDENTIFIER",
	"INTEGER",
	"SECTION",
	"ASSIGN",
	"PLUS",
	"RETURN",
	"VAR",
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
							 strncmp(str, "u1", 	max(siz, 2)) == 0 ||
							 strncmp(str, "u2", 	max(siz, 2)) == 0 ||
							 strncmp(str, "u4", 	max(siz, 2)) == 0 ||
							 strncmp(str, "u8", 	max(siz, 2)) == 0 ||
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

void
ast_new_branch(ast_t *root) {
	if (root->branches_cap <= root->branches_siz) {
		if (!root->branches) {
			root->branches_cap = 1;
			root->branches = malloc(sizeof(ast_t));
		} else {
			root->branches_cap *= 2;
			root->branches = realloc(root->branches, sizeof(ast_t) * root->branches_cap);
		}
	}
	root->branches[root->branches_siz].hstat = root->stat;
	root->branches[root->branches_siz].stat = root->stat;
	root->branches[root->branches_siz].branches = NULL;
	root->branches[root->branches_siz].branches_siz = 0;
	root->branches[root->branches_siz].branches_cap = 0;
	root->top_branch = &(root->branches[root->branches_siz]);
	root->branches_siz++;
}

token_t *
ast_handle_operator_with_type(int type, ast_t **cur_branch, token_t *tkn) {
	token_t *operator, *left_operand, *right_operand;
	ast_t *branch = *cur_branch;
	// TODO: verify if the identifier is undeclared instead of assuming it's not
	if (!tkn->nxt) {
		branch->type = type;
		branch->value.str = tkn->str;
		branch->value.siz = tkn->siz;
	} else {
		switch (tkn->nxt->type) {
		case TKN_OPERATOR:
			if (!tkn->nxt->nxt) {
				fprintf(stderr, "ERROR: unary operators are not handled yet\n"); 
				exit(1);
			}
			switch(tkn->nxt->nxt->type) {
			case TKN_INTEGER:
			case TKN_IDENTIFIER:
				left_operand = tkn;
				operator = tkn->nxt;
				right_operand = tkn->nxt->nxt;
				if (strncmp(operator->str, "=", operator->siz) == 0) {
					branch->type = AST_ASSIGN;
				} else if(strncmp(operator->str, "+", operator->siz) == 0) {
					branch->type = AST_PLUS;
				} else {
					fprintf(stderr, "ERROR: operator '%.*s' not handled\n", operator->siz, operator->str); 
					exit(1);
				}
				ast_new_branch(branch);
				branch->top_branch->type = type;
				branch->top_branch->value.str = left_operand->str;
				branch->top_branch->value.siz = left_operand->siz;

				ast_new_branch(branch);
				if (right_operand->nxt) {
					branch->top_branch->type = AST_SECTION;
					branch = branch->top_branch;
					tkn = operator;
				} else {
					if (right_operand->type == TKN_IDENTIFIER) {
						branch->top_branch->type = AST_IDENTIFIER;
					} else if (right_operand->type == TKN_INTEGER) {
						branch->top_branch->type = AST_INTEGER;
					} else {
						fprintf(stderr, "ERROR: operand '%.*s' not handled\n", right_operand->siz, right_operand->str); 
						exit(1);
					}
					branch->top_branch->value.str = right_operand->str;
					branch->top_branch->value.siz = right_operand->siz;
					tkn = right_operand;
				}
				break;
			default:
				fprintf(stderr, "ERROR: '%.*s' is not a valid operand\n", tkn->siz, tkn->str); 
				exit(1);
				break;
			}
			break;
		default:
			fprintf(stderr, "ERROR: expected ';' before '%.*s'\n", tkn->siz, tkn->str); 
			exit(1);
			break;
		}
	}
	*cur_branch = branch;
	return tkn;
}

ast_t
statements_to_ast() {
	ast_t root;
	root.branches = NULL;
	root.branches_siz = 0;
	root.branches_cap = 0;
	root.type = AST_PROGRAM;

	root.stat = NULL;
	root.hstat = NULL;
	int count;
	do {
		root.stat = tokens_to_statements(root.stat);
		if (!root.hstat && root.stat) root.hstat = root.stat;
		count++;
	} while(root.stat);

	root.stat = root.hstat;
	token_t *tkn = root.stat->htkn;
	ast_new_branch(&root);
	ast_t *branch = root.top_branch;

	// TODO: semicolon error handling
	while (root.stat) {
		switch(segment) {
		case SEG_LOGIC:
			switch(tkn->type) {
			case TKN_SEGMENT: change_segment(tkn); break;
			case TKN_KEYWORD:
				branch->type = AST_RETURN;
				if (tkn->nxt) {
					ast_new_branch(branch);
					branch->top_branch->type = AST_SECTION;
					branch = branch->top_branch;
				}
				break;
			case TKN_INTEGER:
				tkn=ast_handle_operator_with_type(AST_INTEGER, &branch, tkn);
				break;
			case TKN_IDENTIFIER:
				tkn=ast_handle_operator_with_type(AST_IDENTIFIER, &branch, tkn);
				break;
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
				if (tkn->nxt->nxt) {
					fprintf(stderr, "ERROR: expected ';' before '%.*s'\n", tkn->nxt->nxt->siz, tkn->nxt->nxt->str); 
					exit(1);
				}
				branch->type = AST_VARDEF;

				ast_new_branch(branch);
				branch->top_branch->type = AST_TYPE;
				branch->top_branch->value.str = tkn->str;
				branch->top_branch->value.siz = tkn->siz;

				tkn = tkn->nxt;

				ast_new_branch(branch);
				branch->top_branch->type = AST_IDENTIFIER;
				branch->top_branch->value.str = tkn->str;
				branch->top_branch->value.siz = tkn->siz;
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
			int is_segment = root.stat->htkn->type == TKN_SEGMENT;
			root.stat = root.stat->nxt;
			if (!root.stat) continue;
			tkn = root.stat->htkn;
			if (is_segment) continue;
			ast_new_branch(&root);
			branch = root.top_branch;
		}
	}

	return root;
}

void
print_ast(ast_t root, int depth) {
	if (root.branches) {
		for(int i = 0; i < depth; i++) printf("  ");
		printf("%s {\n", ast_type_str[root.type]);
		for (unsigned int i = 0; i < root.branches_siz; i++) {
			print_ast(root.branches[i], depth + 1);
		}
		for(int i = 0; i < depth; i++) printf("  ");
		printf("}\n");
	} else {
		for(int i = 0; i < depth; i++) printf("  ");
		printf("%s : %.*s\n", ast_type_str[root.type], root.value.siz, root.value.str);
	}
}

void
free_ast(ast_t ast) {
	if (!ast.branches) return;
	for (unsigned int i = 0; i < ast.branches_siz; i++) {
		free_ast(ast.branches[i]);
	}
	free(ast.branches);
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


	ast_t ast = statements_to_ast();

	print_ast(ast, 0);

	while (ast.hstat) {
		while (ast.hstat->htkn){
			ast.hstat->tkn =  ast.hstat->htkn;
			ast.hstat->htkn = ast.hstat->htkn->nxt;
			free(ast.hstat->tkn);
		}
		ast.stat =  ast.hstat;
		ast.hstat = ast.hstat->nxt;
		free(ast.stat);
	}
	free_ast(ast);

	src -= f_siz;
	free(src);
	return 0;
}

