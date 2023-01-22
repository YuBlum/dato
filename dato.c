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
		TKN_COUNT,
	}  type;
	char *str;
	int siz;
	unsigned int precedence;
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
	// NOTE: only support signed integers and binary operators for now
	enum {
		AST_PROGRAM,
		AST_VARDEF,
		AST_TYPE,
		AST_IDENTIFIER,
		AST_INTEGER,
		AST_ASSIGN,
		AST_ADD,
		AST_SUB,
		AST_MUL,
		AST_DIV,
		AST_RETURN,
		AST_COUNT,
	} type;

	struct ast *branch;
	struct ast *hbranch;
	struct ast *nxt;
	struct ast *prv;
	struct ast *root;

	token_t *tkn;

	statement_t *stt;
	statement_t *hstt;
} ast_t;

static const char *const ast_type_str[] = {
	"AST_PROGRAM",
	"AST_VARDEF",
	"AST_TYPE",
	"AST_IDENTIFIER",
	"AST_INTEGER",
	"AST_ASSIGN",
	"AST_ADD",
	"AST_SUB",
	"AST_MUL",
	"AST_DIV",
	"AST_RETURN",
};

static char *src;
unsigned int f_siz;
static const char *const empty  = " \t\n";
static const char *const number = "0123456789";
static const char *const letter = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static enum {
	SEG_LOGIC,
	SEG_DATA,
	SEG_SYSTEM,
	SEG_LAYOUT,
} segment = SEG_LOGIC;

typedef struct {
	char *buf;
	unsigned int siz;
} string_t;

void
get_source(int argc, char **argv) {
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
	f_siz = ftell(f);
	fseek(f, 0, SEEK_SET);
	src = malloc(f_siz + 1);
	src[f_siz] = '\0';
	fread(src, f_siz, 1, f);
	fclose(f);
}

void
print_token(token_t *tkn) {
	printf("%s %.*s", token_type_str[tkn->type], tkn->siz, tkn->str);
}

void
next_token(token_t **prv) {
	while (strchr(empty, src[0])) {
		if (src[0] == '\0') { *prv = NULL; return; }
		src++;
	}
	if (*prv && (*prv)->type == TKN_SEGMENT) { *prv = NULL; return; }
	char *str = src;
	int siz, type;
	unsigned int precedence;

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
		} else if (strncmp(str, "+", siz) == 0) {
			type = TKN_OPERATOR;
			precedence = 1;
		} else if (strncmp(str, "-", siz) == 0) { 
			type = TKN_OPERATOR;
			precedence = 1;
		} else if (strncmp(str, "*", siz) == 0) { 
			type = TKN_OPERATOR;
			precedence = 2;
		} else if (strncmp(str, "/", siz) == 0) { 
			type = TKN_OPERATOR;
			precedence = 2;
		} else if (strncmp(str, "=", siz) == 0) {
			type = TKN_OPERATOR;
			precedence = 0;
		} else {
			type = TKN_UNKNOWN;
		}
	}
	if (type == TKN_SEMICOLON) { *prv = NULL; return; }
	token_t *tkn = malloc(sizeof(token_t));
	tkn->siz = siz;
	tkn->str = str;
	tkn->type = type;
	tkn->nxt = NULL;
	tkn->precedence = precedence;
	
	if (*prv) (*prv)->nxt = tkn;
	*prv = tkn;
}

void
lex(statement_t **prv) {
	if (src[0] == '\0') { *prv = NULL; return; }
	token_t *tkn = NULL;
	token_t *htkn = NULL;

	do {
		next_token(&tkn);
		if (!htkn) htkn = tkn;
	} while (tkn);

	if (htkn == NULL) { *prv = NULL; return; }

	statement_t *stt = malloc(sizeof(statement_t));
	stt->tkn = tkn;
	stt->htkn = htkn;
	stt->nxt = NULL;
	//print_statement(stt);

	if (*prv) (*prv)->nxt = stt;
	*prv = stt;
}

void
print_statement(statement_t *stt) {
	printf("{ ");
	stt->tkn = stt->htkn;
	while(stt->tkn) {
		print_token(stt->tkn);
		if (stt->tkn->nxt) {
			printf(", ");
		}
		stt->tkn = stt->tkn->nxt;
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
		exit(1); 
	}
}

ast_t *
ast_new_branch(ast_t *root, token_t *tkn) {
	if (!root) {
		fprintf(stderr, "ERROR: trying to create a new branch, but root is NULL\n");
		exit(1);
	}
	ast_t *new_branch = malloc(sizeof(ast_t));
	new_branch->hstt = root->stt;
	new_branch->stt = root->stt;
	new_branch->branch = NULL;
	new_branch->hbranch = NULL;
	new_branch->root = root;
	new_branch->nxt = NULL;
	new_branch->prv = NULL;
	new_branch->tkn = tkn;

	if (!root->branch) {
		root->branch = new_branch;
		root->hbranch = new_branch;
	} else {
		root->branch->nxt = new_branch;
		new_branch->prv = root->branch;
		root->branch = new_branch;
	}
	return new_branch;
}

void
ast_branch_change_root(ast_t *branch, ast_t *new_root) {
	ast_t *prv_root = branch->root;
	if (branch->prv) {
		branch->prv->nxt = branch->nxt;
	} else {
		prv_root->hbranch = branch->nxt;
	}
	if (branch->nxt) {
		branch->nxt->prv = branch->prv;
	}
	branch->nxt = NULL;
	branch->prv = new_root->branch;
	branch->root = new_root;
	if (branch->prv) {
		branch->prv->nxt = branch;
	} else {
		new_root->branch = branch;
		new_root->hbranch = branch;
	}
}

void
parse_expression(ast_t *root, token_t **out_tkn) {
	if (!root) {
		fprintf(stderr, "ERROR: trying to parse a expression, but root is NULL\n");
		exit(1);
	}
	if (!out_tkn || !*out_tkn) {
		fprintf(stderr, "ERROR: trying to parse a expression, but token is NULL\n");
		exit(1);
	}
	token_t *tkn = *out_tkn;
	ast_t *lhs = root->branch, *expr = ast_new_branch(root, tkn);

	switch(tkn->type) {
		case TKN_INTEGER:
				expr->type = AST_INTEGER;
			break;
		case TKN_IDENTIFIER:
				expr->type = AST_IDENTIFIER;
			break;
		case TKN_OPERATOR:
			if (!lhs) {
				fprintf(stderr, "ERROR: %.*s without a left hand side\n", tkn->siz, tkn->str);
				exit(1);
			}
			if (lhs->type != AST_INTEGER && lhs->type != AST_IDENTIFIER && !(lhs->tkn && lhs->tkn->type == TKN_OPERATOR)) {
				if (lhs->tkn) fprintf(stderr, "ERROR: %.*s", lhs->tkn->siz, lhs->tkn->str);
				else					fprintf(stderr, "ERROR: %s", ast_type_str[lhs->type]);
				fprintf(stderr, " isn't valid as left hand side of %.*s\n", tkn->siz, tkn->str);
				exit(1);
			}
			if (!tkn->nxt) {
				fprintf(stderr, "ERROR: %.*s without a right hand side\n", tkn->siz, tkn->str);
				exit(1);
			}
			if (tkn->nxt->type != TKN_INTEGER && tkn->nxt->type != TKN_IDENTIFIER) {
				fprintf(stderr, "ERROR: %.*s isn't valid as right hand side of %.*s\n", tkn->nxt->siz, tkn->nxt->str, tkn->siz, tkn->str);
				exit(1);
			}
			if (lhs->tkn && lhs->tkn->type == TKN_OPERATOR && lhs->tkn->precedence < tkn->precedence) assert(0 && "parse_expression: unreacheble");
			if (strncmp("=", tkn->str, tkn->siz) == 0) {
				expr->type = AST_ASSIGN;
			} else if (strncmp("+", tkn->str, tkn->siz) == 0) {
				expr->type = AST_ADD;
			} else if (strncmp("-", tkn->str, tkn->siz) == 0) {
				expr->type = AST_SUB;
			} else if (strncmp("*", tkn->str, tkn->siz) == 0) {
				expr->type = AST_MUL;
			} else if (strncmp("/", tkn->str, tkn->siz) == 0) {
				expr->type = AST_DIV;
			} else {
				fprintf(stderr, "ERROR: operator '%.*s' is not handled\n", tkn->siz, tkn->str);
				exit(1);
			}
			ast_branch_change_root(lhs, expr);
			*out_tkn = tkn->nxt;
			parse_expression(expr, out_tkn);
			if (tkn->nxt->nxt && tkn->nxt->nxt->type == TKN_OPERATOR) {
				*out_tkn = tkn->nxt->nxt;
				if (tkn->nxt->nxt->precedence > tkn->precedence) parse_expression(expr, out_tkn);
				else {
					parse_expression(expr->root, out_tkn);
				}
			}
			break;
		default:
			fprintf(stderr, "ERROR: %.*s is not valid as an expression\n", tkn->siz, tkn->str);
			exit(1);
			break;
	}
}

void
parse_variable_declaration(ast_t *root, token_t **out_tkn) {
	if (!root) {
		fprintf(stderr, "ERROR: trying to parse a variable declaration, but root is NULL\n");
		exit(1);
	}
	if (!out_tkn || !*out_tkn) {
		fprintf(stderr, "ERROR: trying to parse a variable declaration, but token is NULL\n");
		exit(1);
	}
	token_t *tkn = *out_tkn;
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
	ast_t *vardef = ast_new_branch(root, NULL);
	vardef->type = AST_VARDEF;

	ast_t *vartype = ast_new_branch(vardef, tkn);
	vartype->type = AST_TYPE;
	tkn = tkn->nxt;
	parse_expression(vardef, &tkn);
	*out_tkn = tkn;
}

void
parse_keyword(ast_t **out_root, token_t *tkn) {
	if (!out_root || !*out_root) {
		fprintf(stderr, "ERROR: trying to parse a variable declaration, but root is NULL\n");
		exit(1);
	}
	if (!tkn) {
		fprintf(stderr, "ERROR: trying to parse a variable declaration, but token is NULL\n");
		exit(1);
	}
	ast_t *root = *out_root;
	if (strncmp(tkn->str, "ret", max(3, tkn->siz)) == 0) {
		ast_t *ret = ast_new_branch(root, NULL);
		ret->type = AST_RETURN;
		if (tkn->nxt) {
			root = ret;
		}
	} else {
		fprintf(stderr, "ERROR: keyword '%.*s' is not handled\n", tkn->siz, tkn->str);
		exit(1);
	}
	*out_root = root;
}

ast_t *
parse(void) {
	ast_t *root = malloc(sizeof(ast_t));
	root->type = AST_PROGRAM;
	root->branch = NULL;
	root->hbranch = NULL;
	root->nxt = NULL;
	root->prv = NULL;
	root->root = NULL;

	root->stt = NULL;
	root->hstt = NULL;
	int count = 0;
	do {
		lex(&root->stt);
		//if (root->stt) print_statement(root->stt);
		if (!root->hstt) root->hstt = root->stt;
		count++;
	} while(root->stt);

	root->stt = root->hstt;
	token_t *tkn = root->stt->htkn;
	ast_t *branch = root;

	// TODO: semicolon error handling
	while (root->stt) {
		switch(segment) {
			case SEG_LOGIC:
				switch(tkn->type) {
				case TKN_SEGMENT: change_segment(tkn); break;
				case TKN_KEYWORD:
					parse_keyword(&branch, tkn);
					break;
				case TKN_IDENTIFIER:
				case TKN_INTEGER:
				case TKN_OPERATOR:
					parse_expression(branch, &tkn);
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
					parse_variable_declaration(branch, &tkn);
					break;
				default: 
					fprintf(stderr, "ERROR: '%d a.k.a %.*s' is not handled in 'data'\n", tkn->type, tkn->siz, tkn->str); 
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
			root->stt = root->stt->nxt;
			if (!root->stt) continue;
			tkn = root->stt->htkn;
			branch = root;
		}
	}

	return root;
}

void
print_ast(ast_t *root, int depth) {
	if (root->branch) {
		for(int i = 0; i < depth; i++) printf("  ");
		printf("%s {\n", ast_type_str[root->type]);
		ast_t *branch = root->hbranch;
		while (branch) {
			print_ast(branch, depth + 1);
			branch = branch->nxt;
		}
		for(int i = 0; i < depth; i++) printf("  ");
		printf("}\n");
	} else {
		for(int i = 0; i < depth; i++) printf("  ");
		printf("%s", ast_type_str[root->type]);
		if (root->tkn) printf(" : %.*s", root->tkn->siz, root->tkn->str);
		putchar('\n');
	}
}

void
free_ast(ast_t *root) {
	while (root->hbranch) {
		root->branch = root->hbranch;
		root->hbranch = root->hbranch->nxt;
		free_ast(root->branch);
	}
	if (root->type == AST_PROGRAM) {
		while (root->hstt) {
			root->stt = root->hstt;
			while (root->stt->htkn) {
				root->stt->tkn = root->stt->htkn;
				root->stt->htkn = root->stt->htkn->nxt;
				free(root->stt->tkn);
			}
			root->hstt = root->hstt->nxt;
			free(root->stt);
		}
	}
	free(root);
}

/* djb2 */
unsigned int
hash(char *str, unsigned int siz) {
	unsigned int h = 5381;
	for (unsigned int i = 0; i < siz; i++) h = ((h << 5) + h) ^ str[i];
	return h;
}

typedef struct identifier {
	enum {
		ID_VARIABLE,
		ID_SYSTEM,
		ID_ANY,
		ID_COUNT
	} type;
	char *str;
	union {
		unsigned int datatype;
		unsigned int returntype;
	};
	unsigned int siz;
	struct identifier *nxt;
} identifier_t;

static const char *const identifier_type_str[] = {
	"ID_VARIABLE",
	"ID_SYSTEM",
};

static identifier_t **ids;
static unsigned int ids_count;
static unsigned int ids_cap = 3;

void
set_identifier(identifier_t **ids, unsigned int ids_cap, identifier_t *id) {
	if (!ids_cap) {
		fprintf(stderr, "ERROR: trying to set identifier, but ids capacity is 0\n");
		exit(1);
	}
	if (!id) {
		fprintf(stderr, "ERROR: trying to set identifier, but id is NULL\n");
		exit(1);
	}
	if (!ids) {
		fprintf(stderr, "ERROR: trying to set identifier, but ids is NULL\n");
		exit(1);
	}
	id->nxt = NULL;
	unsigned int idx = hash(id->str, id->siz) % ids_cap;
	if (ids[idx] != NULL) id->nxt = ids[idx];
	ids[idx] = id;
}

void
ids_resize(void) {
	unsigned int new_ids_cap = ids_cap * 2 + 1;
	identifier_t **new_ids = malloc(sizeof(identifier_t *) * new_ids_cap);
	memset(new_ids, 0, sizeof(identifier_t *) * new_ids_cap);
	if (ids != NULL) {
		for (unsigned int i = 0; i < ids_cap; i++) {
			if (ids[i] == NULL) continue;
			while(ids[i]) {
				identifier_t *nxt = ids[i]->nxt;
				set_identifier(new_ids, new_ids_cap, ids[i]);
				ids[i] = nxt;
			}
		}
		free(ids);
	}
	ids_cap = new_ids_cap;
	ids = new_ids;
}

identifier_t *
get_identifier(unsigned int type, char *str, unsigned int siz) {
	if (!siz) {
		fprintf(stderr, "ERROR: trying to remove identifier, but string size is 0\n");
		exit(1);
	}
	if (!str) {
		fprintf(stderr, "ERROR: trying to remove identifier, but string is NULL\n");
		exit(1);
	}
	unsigned int idx = hash(str, siz) % ids_cap;
	identifier_t *id = ids[idx];
	while (id) {
		if (strncmp(id->str, str, max(id->siz, siz)) == 0 && (type == ID_ANY || id->type == type)) break;
		id = id->nxt;
	}
	return id;
}

identifier_t *
add_identifier(unsigned int type, char *str, unsigned int siz) {
	if (!siz) {
		fprintf(stderr, "ERROR: trying to add new identifier, but string size is 0\n");
		exit(1);
	}
	if (!str) {
		fprintf(stderr, "ERROR: trying to add new identifier, but string is NULL\n");
		exit(1);
	}
	if (ids == NULL) {
		ids_resize();
	}
	identifier_t *id = get_identifier(type, str, siz);
	if (!id) {
		ids_count++;
		if (ids_count / (float)ids_cap > 0.8f) ids_resize();
		id = malloc(sizeof(identifier_t));
		id->type = type;
		id->str = str;
		id->siz = siz;
		set_identifier(ids, ids_cap, id);
	}
	return id;
}

void
remove_identifier(unsigned int type, char *str, unsigned int siz) {
	if (!siz) {
		fprintf(stderr, "ERROR: trying to remove identifier, but string size is 0\n");
		exit(1);
	}
	if (!str) {
		fprintf(stderr, "ERROR: trying to remove identifier, but string is NULL\n");
		exit(1);
	}
	unsigned int idx = hash(str, siz) % ids_cap;
	identifier_t *id = ids[idx];
	identifier_t *prv = NULL;
	while (id) {
		if (strncmp(id->str, str, max(id->siz, siz)) == 0 && (type == ID_ANY || id->type == type)) {
			if (!prv) {
				ids[idx] = id->nxt;
			} else {
				prv->nxt = id->nxt;
			}
			free(id);
			ids_count--;
			break;
		}
		prv = id;
		id = id->nxt;
	}
	if (!id) {
		fprintf(stderr, "ERROR: trying to remove identifier '%.*s', but it doesn't exists\n", siz, str);
		exit(1);
	}
}

void
print_ids(void) {
	for (unsigned int i = 0; i < ids_cap; i++) {
		printf("ids[%u] = { ", i);
		if (ids[i] != NULL) {
			identifier_t *id = ids[i];
			while (id) {
				printf("{ %s: %.*s }", identifier_type_str[id->type], id->siz, id->str);
				id = id->nxt;
				if (id) printf(", ");
			}
		} else {
				printf("NULL");
		}
		printf(" }\n");
	}
}

void
string_cat(string_t *str, string_t src) {
	unsigned int idx_modify = str->siz;
	str->siz += src.siz;
	str->buf = realloc(str->buf, str->siz);
	for (unsigned int i = 0; i < src.siz; i++) {
		str->buf[idx_modify + i] = src.buf[i];
	}
}

#define string(str, siz) ((string_t){ (str), (siz) })
#define cstring(cstr) ((string_t){ (cstr), strlen((cstr)) })
#define cstring_cat(str, src) (string_cat(str, cstring(src)))

/* DOIL - DatO Intermediate Language */
typedef struct {
	union {
		unsigned int reg;
		string_t cst;
	} val;
	int is_reg;
	int unused;
} reg_or_const;

typedef struct instruction {
	enum {
		DOIL_ADD,
		DOIL_SUB,
		DOIL_MUL,
		DOIL_DIV,
		DOIL_DEF,
		DOIL_MOV,
		DOIL_SET,
		DOIL_GET,
		DOIL_RET,
	} type;
	union {
		struct {
			unsigned int lhs;
			unsigned int rhs;
			unsigned int dst;
		} ope; /*ope r0 r1 r2 = (r2 = r0 ? r1)*/
		struct {
			string_t name;
			enum {
				DOIL_BYTE,
				DOIL_WORD,
				DOIL_DWORD,
				DOIL_QWORD,
			} type;
		} def; /*def x u8 = (var x: u8)*/
		struct {
			unsigned int reg;
			string_t val;
		} mov; /*mov r0 10 = (r0 = 10)*/
		struct {
			reg_or_const dst;
			reg_or_const src;
		} set; /*set x 10 = (x = 10)*/
		struct {
			string_t src;
			unsigned int reg;
		} get; /*get x r0 = (r0 = x)*/
		struct {
			reg_or_const src;
		} ret; /*ret 20 | ret*/
	};

	struct instruction *nxt;
} instruction_t;

const char *const doil_datatype_str[] = {
	"byte",
	"word",
	"dword",
	"qword",
};

typedef struct {
	int *registers;
	unsigned int registers_count;
	unsigned int registers_cap;
	instruction_t *ins;
	instruction_t *hins;
} doil_t;

#define INTEGER_STRING_MAX 20

instruction_t *
doil_make_instruction(doil_t *doil, unsigned int type) {
	instruction_t *ins = malloc(sizeof(instruction_t));
	*ins = (instruction_t){0};
	ins->type = type;
	if (doil->ins) doil->ins->nxt = ins;
	doil->ins = ins;
	if (!doil->hins) doil->hins = ins;
	return ins;
}

unsigned int
doil_get_register(doil_t *doil) {
	for (unsigned int i = 0; i < doil->registers_count; i++) {
		if (!doil->registers[i]) {
			doil->registers[i] = 1;
			return i;
		}
	}
	if (doil->registers_cap <= doil->registers_count) {
		doil->registers_cap = !doil->registers_cap ? 10 : doil->registers_cap * 2;
		doil->registers = !doil->registers ? malloc(sizeof(ast_t) * doil->registers_cap) : realloc(doil->registers, doil->registers_cap);
		for (unsigned int i = doil->registers_count; i < doil->registers_cap; i++) doil->registers[i] = 0;
	}
	doil->registers[doil->registers_count] = 1;
	return doil->registers_count++;
}

void
doil_clear_register(doil_t *doil, int register_index) {
	doil->registers[register_index] = 0;
}

void
dato_variable_definition_to_doil(doil_t *doil, ast_t *def) {
	token_t *type = def->hbranch->tkn,
					*id 	= def->hbranch->nxt->tkn;
	instruction_t *ins = doil_make_instruction(doil, DOIL_DEF);
	ins->def.name = string(id->str, id->siz);
	/*TODO: add a struct for types like the identifiers, for now they are all hard coded and unsigned*/
	if (strncmp(type->str, "i1", max(type->siz, 2)) == 0 || strncmp(type->str, "u1", max(type->siz, 2)) == 0) {
		ins->def.type = DOIL_BYTE;
	} else if (strncmp(type->str, "i2", max(type->siz, 2)) == 0 || strncmp(type->str, "u2", max(type->siz, 2)) == 0) {
		ins->def.type = DOIL_WORD;
	} else if (strncmp(type->str, "i4", max(type->siz, 2)) == 0 || strncmp(type->str, "u4", max(type->siz, 2)) == 0) {
		ins->def.type = DOIL_DWORD;
	} else if (strncmp(type->str, "i8", max(type->siz, 2)) == 0 || strncmp(type->str, "u8", max(type->siz, 2)) == 0) {
		ins->def.type = DOIL_QWORD;
	} else {
		fprintf(stderr, "ERROR: type '%.*s' not supported\n", type->siz, type->str);
		exit(1);
	}
}

unsigned int dato_assignment_to_doil(doil_t *doil, ast_t *asg);
unsigned int dato_expression_to_doil(doil_t *doil, ast_t *exp);

unsigned int
dato_expression_operator_to_doil(doil_t *doil, ast_t *exp, unsigned int operator) {
	ast_t *lhs = exp->hbranch;
	ast_t *rhs = exp->hbranch->nxt;
	
	instruction_t *ins = doil_make_instruction(doil, operator);

	ins->ope.lhs = dato_expression_to_doil(doil, lhs);
	ins->ope.rhs = dato_expression_to_doil(doil, rhs);
	ins->ope.dst = ins->ope.lhs;
	
	doil_clear_register(doil, ins->ope.rhs);
	return ins->ope.lhs;
}

unsigned int
dato_expression_to_doil(doil_t *doil, ast_t *exp) {
	unsigned int register_index;

	instruction_t *ins;

	switch (exp->type) {
		case AST_ADD:
			register_index = dato_expression_operator_to_doil(doil, exp, DOIL_ADD);
			break;
		case AST_SUB:
			register_index = dato_expression_operator_to_doil(doil, exp, DOIL_SUB);
			break;
		case AST_MUL:
			register_index = dato_expression_operator_to_doil(doil, exp, DOIL_MUL);
			break;
		case AST_DIV:
			register_index = dato_expression_operator_to_doil(doil, exp, DOIL_DIV);
			break;
		case AST_ASSIGN:
			register_index = dato_assignment_to_doil(doil, exp);
			break;
		case AST_IDENTIFIER:
			ins = doil_make_instruction(doil, DOIL_GET);
			ins->get.src = string(exp->tkn->str, exp->tkn->siz);
			ins->get.reg = doil_get_register(doil);
			register_index = ins->get.reg;
			break;
		case AST_INTEGER:
			ins = doil_make_instruction(doil, DOIL_MOV);
			ins->mov.reg = doil_get_register(doil);
			ins->mov.val = string(exp->tkn->str, exp->tkn->siz);;
			register_index = ins->mov.reg;
			break;
		default:
			fprintf(stderr, "ERROR: '%s' is not a valid expression\n", ast_type_str[exp->type]);
			exit(1);
	}
	return register_index;
}

unsigned int
dato_assignment_to_doil(doil_t *doil, ast_t *asg) {
	ast_t *id  = asg->hbranch;
	ast_t *val = asg->hbranch->nxt;

	instruction_t *ins = doil_make_instruction(doil, DOIL_SET);
	if (id->type == AST_IDENTIFIER) {
		ins->set.dst.val.cst = string(id->tkn->str, id->tkn->siz);
	} else {
		ins->set.dst.val.reg = dato_expression_to_doil(doil, id);
		ins->set.dst.is_reg = 1;
		doil_clear_register(doil, ins->set.dst.val.reg);
	}

	unsigned int val_register = dato_expression_to_doil(doil, val);
	if (val->type == AST_INTEGER) {
		ins->set.src.val.cst = string(val->tkn->str, val->tkn->siz);
	} else {
		ins->set.src.val.reg = val_register;
		ins->set.src.is_reg = 1;
	}

	return val_register;
}

void
dato_return_to_doil(doil_t *doil, ast_t *ret) {
	ast_t *val = ret->branch;
	instruction_t *ins;

	if (!val) {
		ins = doil_make_instruction(doil, DOIL_RET);
		ins->ret.src.unused = 1;
		return;
	}

	ins = doil_make_instruction(doil, DOIL_RET);;
	if (val->type == AST_INTEGER) {
		ins->ret.src.val.cst = string(val->tkn->str, val->tkn->siz);
	} else {
		ins->ret.src.val.reg = dato_expression_to_doil(doil, val);
		ins->ret.src.is_reg = 1;
		doil_clear_register(doil, ins->ret.src.val.reg);
	}
}

doil_t
doil_lex(ast_t *root) {
	doil_t doil = {0};
	root->branch = root->hbranch;
	while (root->branch) {
		switch (root->branch->type) {
			case AST_ADD:
			case AST_SUB:
			case AST_MUL:
			case AST_DIV:
			case AST_IDENTIFIER:
			case AST_INTEGER:
				fprintf(stderr, "WARNING: statement with no effect\n");
				break;
			case AST_VARDEF: 
				dato_variable_definition_to_doil(&doil, root->branch);
				break;
			case AST_ASSIGN: 
				doil_clear_register(&doil, dato_assignment_to_doil(&doil, root->branch));
				break;
			case AST_RETURN: 
				dato_return_to_doil(&doil, root->branch);
				break;
			default:
				fprintf(stderr, "ERROR: '%s' is not a valid operation\n", ast_type_str[root->branch->type]);
				exit(1);
		}
		root->branch = root->branch->nxt;
	}
	free(doil.registers);
	free_ast(root);
	return doil;
}

void
doil_remove_unused_identifiers(doil_t *doil) {
	(void)doil;
	assert(0 && "not implemented");
}

void
print_doil(doil_t doil) {
	doil.ins = doil.hins;
	while (doil.ins) {
		switch (doil.ins->type) {
			case DOIL_ADD:
				printf("add r%u %u %u\n", doil.ins->ope.lhs, doil.ins->ope.rhs, doil.ins->ope.dst);
				break;
			case DOIL_SUB:
				printf("sub r%u %u %u\n", doil.ins->ope.lhs, doil.ins->ope.rhs, doil.ins->ope.dst);
				break;
			case DOIL_MUL:
				printf("mul r%u %u %u\n", doil.ins->ope.lhs, doil.ins->ope.rhs, doil.ins->ope.dst);
				break;
			case DOIL_DIV:
				printf("div r%u %u %u\n", doil.ins->ope.lhs, doil.ins->ope.rhs, doil.ins->ope.dst);
				break;
			case DOIL_DEF:
				printf("def %.*s %s\n", doil.ins->def.name.siz, doil.ins->def.name.buf, doil_datatype_str[doil.ins->def.type]);
				break;
			case DOIL_MOV:
				printf("mov r%u %.*s\n", doil.ins->mov.reg, doil.ins->mov.val.siz, doil.ins->mov.val.buf);
				break;
			case DOIL_SET:
				if (doil.ins->set.dst.is_reg) {
					printf("set r%u", doil.ins->set.dst.val.reg);
				} else {
					printf("set %.*s", doil.ins->set.dst.val.cst.siz, doil.ins->set.dst.val.cst.buf);
				}
				if (doil.ins->set.src.is_reg) {
					printf(" r%u\n", doil.ins->set.src.val.reg);
				} else {
						printf(" %.*s\n", doil.ins->set.src.val.cst.siz, doil.ins->set.src.val.cst.buf);
				}
				break;
			case DOIL_GET:
				printf("get %.*s r%u\n", doil.ins->get.src.siz, doil.ins->get.src.buf, doil.ins->get.reg);
				break;
			case DOIL_RET:
				printf("ret");
				if (!doil.ins->ret.src.unused) {
					if (doil.ins->ret.src.is_reg) {
						printf(" r%u\n", doil.ins->ret.src.val.reg);
					} else {
						printf(" %.*s\n", doil.ins->ret.src.val.cst.siz, doil.ins->ret.src.val.cst.buf);
					}
				} else {
					putchar('\n');
				}
				break;
			default:
				fprintf(stderr, "ERROR: not a valid instruction\n");
				exit(1);
		}
		doil.ins = doil.ins->nxt;
	}
}

void
doil_clean_up(doil_t doil) {
	while (doil.hins) {
		doil.ins = doil.hins;
		doil.hins = doil.hins->nxt;
		free(doil.ins);
	}
	src -= f_siz;
	free(src);
}

/* generate doil code from dato code */
doil_t
front_end(void) {
	ast_t *root = parse();
	doil_t doil = doil_lex(root);
	print_doil(doil);
	doil_remove_unused_identifiers(&doil);
	printf("------");
	print_doil(doil);
	return doil;
}

void
linux_x86_64(doil_t doil) {
	(void)doil;
	assert(0 && "not implemented");
}

/* generate an executable from doil code */
void
back_end(doil_t doil) {
#if defined(__linux__) && defined(__x86_64__)
	linux_x86_64(doil);
#else
	fprintf(stderr, "ERROR: dato only supports linux x86_64 operating systems\n");
	exit(1);
#endif
}

int
main(int argc, char **argv) {
	get_source(argc, argv);
	doil_t doil = front_end();
	doil_clean_up(doil);
	//back_end(doil);
	return 0;
}
