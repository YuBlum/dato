#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

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
		AST_SECTION,
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
	"AST_SECTION",
	"AST_ASSIGN",
	"AST_ADD",
	"AST_SUB",
	"AST_MUL",
	"AST_DIV",
	"AST_RETURN",
};

static char *src;
static const char *const empty  = " \t\n";
static const char *const number = "0123456789";
static const char *const letter = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static enum {
	SEG_LOGIC,
	SEG_DATA,
	SEG_SYSTEM,
	SEG_LAYOUT,
} segment = SEG_LOGIC;

typedef struct memory_block {
	unsigned int size;
	unsigned int offset;
	struct memory_block *nxt;
	struct memory_block *prv;
	int free;
} memory_block_t;

#define ARENA_PAGE 4096
static struct {
	unsigned char *buf;
	unsigned char *limit;
	unsigned int cap;
	unsigned int offset;
	memory_block_t *block;
	memory_block_t *hblock; // header block
	memory_block_t *ublock; // unused block
	memory_block_t *hublock; // header unused block
} arena;

#define BLOCK_IN_BYTES align_to_ptr(sizeof(memory_block_t))

memory_block_t *
arena_find_free(unsigned int amount) {
	memory_block_t *block = NULL;
	block = arena.hblock;
	while(block && (!block->free || block->size < amount)) {
		block = block->nxt;
	}
	return block;
}

unsigned int
align_to_ptr(int x) {
	unsigned int rest = x & (sizeof(void *) - 1);
	if (rest != 0) {
		x += sizeof(void *) - rest;
	}
	return x;
}

void
arena_grow(void) {
	void *top = sbrk(0);
	if (!arena.limit || arena.limit == top) {
		void *tmp = sbrk(ARENA_PAGE);
		if (!arena.buf) arena.buf = tmp;
		arena.cap += ARENA_PAGE;
		arena.limit = sbrk(0);
		//printf("GROW! offset: %u, cap: %u\n", arena.offset, arena.cap);
	} else {
		fprintf(stderr, "ERROR: couldn't grow the arena heap\n");
	}
}

void *
arena_alloc(unsigned int amount) {
	if (amount == 0) amount = 1;
	amount = align_to_ptr(amount);

	void *memory = NULL;
	if (!arena.buf) {
		arena.offset 	= 0;
		arena.cap     = 0;
		arena.block 	= NULL;
		arena.hblock 	= NULL;
		arena.ublock 	= NULL;
		arena.hublock = NULL;
		arena.limit   = NULL;
		arena_grow();
	}

	memory_block_t *free_block = arena_find_free(amount);
	if (free_block) {
		memory = arena.buf + free_block->offset;
		unsigned int diff = free_block->size - amount;
		free_block->size -= diff;
		free_block->free = 0;
		if (diff != 0) {
			if (arena.offset + BLOCK_IN_BYTES > arena.cap) arena_grow();
			memory_block_t *new_block=(memory_block_t *)(arena.buf+arena.offset);
			arena.offset += BLOCK_IN_BYTES;
			new_block->free = 1;
			new_block->size = diff;
			new_block->offset = free_block->offset + free_block->size;
			new_block->prv = free_block;
			new_block->nxt = free_block->nxt;
			free_block->nxt = new_block;
			if (!new_block->nxt) {
				arena.block = new_block;
			}
		}
		return memory;
	}
	if (arena.offset + amount + BLOCK_IN_BYTES > arena.cap) arena_grow();
	memory_block_t *new_block=(memory_block_t *)(arena.buf+arena.offset);
	arena.offset += BLOCK_IN_BYTES;
	new_block->size = amount;
	memory = arena.buf + arena.offset;
	new_block->offset = arena.offset;
	new_block->free = 0;
	new_block->nxt = NULL;
	new_block->prv = arena.block;
	if (arena.block) arena.block->nxt = new_block;
	arena.block = new_block;
	if (!arena.hblock) arena.hblock = new_block;
	arena.offset += amount;
	return memory;
}

int
arena_free(void *memory) {
	if (memory == NULL) goto arena_free_invalid;
	memory_block_t *block = NULL;
	if ((void *)arena.buf > memory||memory >(void *)arena.buf+ARENA_PAGE){
		goto arena_free_invalid;
	}
	block = arena.hblock;
	while(arena.buf + block->offset != memory) {
		block = block->nxt;
		if (block == NULL) goto arena_free_invalid;
	}
	if (block->free) goto arena_free_invalid;
	block->free = 1;
	if (block->nxt && block->nxt->free) {
		memory_block_t *del_block = block->nxt;
		block->nxt = del_block->nxt;
		block->nxt->prv = block;
		if ((void *)del_block==arena.buf + block->offset + block->size) {
			block->size += del_block->size + BLOCK_IN_BYTES;
		} else {
			block->size += del_block->size;
			del_block->nxt = NULL;
			if (!arena.hublock) {
				del_block->prv = NULL;
				arena.hublock = del_block;
				arena.ublock  = del_block;
			} else {
				arena.ublock->nxt = del_block;
			}
		}
	}
	if (block->prv && block->prv->free) {
		memory_block_t *del_block = block;
		block = block->prv;

		if ((void *)del_block==arena.buf + block->offset + block->size) {
			block->size += del_block->size + BLOCK_IN_BYTES;
			block->nxt = del_block->nxt;
		} else {
			block = del_block;
		}
	}
	memory_block_t *ublock = arena.hublock;
	while(ublock) {
		if ((void *)ublock == arena.buf + block->offset + block->size) {
			if (ublock->prv) ublock->prv->nxt = ublock->nxt;
			else {
				arena.hublock = ublock->nxt;
				arena.ublock = ublock->nxt;
			}
			block->size += BLOCK_IN_BYTES;
		}
		if ((void *)ublock == arena.buf + block->offset -BLOCK_IN_BYTES){
			if (ublock->prv) ublock->prv->nxt = ublock->nxt;
			else {
				arena.hublock = ublock->nxt;
				arena.ublock = ublock->nxt;
			}
			memory_block_t *new_block = block - BLOCK_IN_BYTES; 
			new_block->size += block->size + BLOCK_IN_BYTES;
			new_block->nxt = block->nxt;
			new_block->free = 1;
			if (block->prv) {
				new_block->prv = block->prv;
				new_block->prv->nxt = new_block;
			} else {
				new_block->prv = NULL;
				arena.hblock = new_block;
				arena.block  = new_block;
			}
		}
		ublock = ublock->nxt;
	}
	return 0;
arena_free_invalid:
	fprintf(stderr, "ERROR: trying to free invalid memory\n");
	return 1;
}

void *
arena_realloc(void *memory, unsigned int new_size) {
	if (memory == NULL) goto arena_realloc_invalid;
	memory_block_t *block = NULL;
	if ((void *)arena.buf > memory||memory >(void *)arena.buf+ARENA_PAGE){
		goto arena_realloc_invalid;
	}
	block = arena.hblock;
	while(arena.buf + block->offset != memory) {
		block = block->nxt;
		if (block == NULL) goto arena_realloc_invalid;
	}
	if (block->free) goto arena_realloc_invalid;
	if (new_size == 0) new_size = 1;
	new_size = align_to_ptr(new_size);
	if (block->size == new_size) {
		return memory;
	} else if (block->size > new_size) {
		block->size = new_size;
		if (arena.offset + BLOCK_IN_BYTES > arena.cap) arena_grow();
		memory_block_t *new_block=(memory_block_t *)(arena.buf+arena.offset);
		arena.offset += BLOCK_IN_BYTES;
		new_block->free = 1;
		new_block->size = block->size - new_size;
		new_block->offset = block->offset + block->size;
		new_block->prv = block;
		new_block->nxt = block->nxt;
		block->nxt = new_block;
		if (!new_block->nxt) {
			arena.block = new_block;
		}
	} else {
		unsigned char *new_memory = arena_alloc(new_size);
		for (unsigned int i = 0; i < block->size; i++) {
			new_memory[i] = ((unsigned char *)memory)[i];
		}
		arena_free(memory);
		return new_memory;
	}
arena_realloc_invalid:
	fprintf(stderr, "ERROR: trying to reallocate invalid memory\n");
	return NULL;
}

void
arena_destroy(void) {
	if (!arena.buf) return;
	if (arena.limit == sbrk(0)) {
		sbrk(-arena.cap);
		arena.buf = NULL;
	} else {
		fprintf(stderr, "ERROR: couldn't destroy the arena heap\n");
	}
}

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
	long f_siz = ftell(f);
	fseek(f, 0, SEEK_SET);
	src = arena_alloc(f_siz);
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
	token_t *tkn = arena_alloc(sizeof(token_t));
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

	statement_t *stt = arena_alloc(sizeof(statement_t));
	stt->tkn = tkn;
	stt->htkn = htkn;

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
	}
}

ast_t *
ast_new_branch(ast_t *root, token_t *tkn) {
	if (!root) {
		fprintf(stderr, "ERROR: trying to create a new branch, but root is NULL\n");
		exit(1);
	}
	ast_t *new_branch = arena_alloc(sizeof(ast_t));
	new_branch->hstt = root->stt;
	new_branch->stt = root->stt;
	new_branch->branch = NULL;
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
			ast_t *section = ast_new_branch(ret, NULL);
			section->type = AST_SECTION;
			root = section;
		}
	} else {
		fprintf(stderr, "ERROR: keyword '%.*s' is not handled\n", tkn->siz, tkn->str);
		exit(1);
	}
	*out_root = root;
}

ast_t *
parse() {
	ast_t *root = arena_alloc(sizeof(ast_t));
	root->type = AST_PROGRAM;
	root->branch = NULL;
	root->hbranch = NULL;
	root->nxt = NULL;
	root->prv = NULL;

	root->stt = NULL;
	root->hstt = NULL;
	int count = 0;
	do {
		lex(&root->stt);
		//if (root->stt) print_statement(root->stt);
		if (!root->hstt && root->stt) root->hstt = root->stt;
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

int
main(int argc, char **argv) {
	get_source(argc, argv);

	ast_t *ast = parse();
	print_ast(ast, 0);

	arena_destroy();
	return 0;
}
