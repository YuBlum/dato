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
	// only support signed integers for now
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
		AST_VAR,
		AST_COUNT,
	} type;
	union {
		struct {
			char *str;
			int siz;
		};
	} value;
	struct ast *branch;
	struct ast *hbranch;
	struct ast *nxt;
	struct ast *prv;
	struct ast *root;

	unsigned int precedence;

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
	"ADD",
	"SUB",
	"MUL",
	"DIV",
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
	if (type == TKN_SEMICOLON) return NULL;
	token_t *tkn = arena_alloc(sizeof(token_t));
	tkn->siz = siz;
	tkn->str = str;
	tkn->type = type;
	tkn->nxt = NULL;
	tkn->precedence = precedence;
	
	if (prv) prv->nxt = tkn;

	return tkn;
}


statement_t *
tokens_to_statements(statement_t *prv) {
	if (src[0] == '\0') return NULL;
	token_t *tkn = NULL;
	token_t *htkn = NULL;

	do {
		tkn = file_to_tokens(tkn);
		if (tkn && tkn->type == TKN_OPERATOR) {
		}
		if (!htkn && tkn) htkn = tkn;
	} while (tkn);

	if (htkn == NULL) return NULL;

	statement_t *stat = arena_alloc(sizeof(statement_t));
	stat->tkn = tkn;
	stat->htkn = htkn;

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
	if (root == NULL) {
		fprintf(stderr, "ERROR: root is (nil)\n");
		exit(1);
	}
	ast_t *new_branch = arena_alloc(sizeof(ast_t));
	new_branch->hstat = root->stat;
	new_branch->stat = root->stat;
	new_branch->branch = NULL;
	new_branch->root = root;
	new_branch->precedence = 0;
	new_branch->nxt = NULL;
	new_branch->prv = NULL;

	if (!root->branch) {
		root->branch = new_branch;
		root->hbranch = new_branch;
	} else {
		root->branch->nxt = new_branch;
		new_branch->prv = root->branch;
		root->branch = new_branch;
	}
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
token_operator_to_ast_operator(token_t *tkn, ast_t *ast) {
	if (strncmp(tkn->str, "=", tkn->siz) == 0) {
		ast->type = AST_ASSIGN;
	} else if(strncmp(tkn->str, "+", tkn->siz) == 0) {
		ast->type = AST_ADD;
	} else if(strncmp(tkn->str, "*", tkn->siz) == 0) {
		ast->type = AST_MUL;
	} else {
		fprintf(stderr, "ERROR: operator '%.*s' not handled\n", tkn->siz, tkn->str); 
		exit(1);
	}
	ast->precedence = tkn->precedence;
}

token_t *
ast_handle_operator_with_type(int type,ast_t **cur_branch,token_t *tkn){
	token_t *operator, *left_operand, *right_operand;
	ast_t *branch = *cur_branch;
	//TODO: verify if the identifier is declared instead of assuming it is
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
				token_operator_to_ast_operator(operator, branch);

				ast_new_branch(branch);
				branch->branch->type = type;
				branch->branch->value.str = left_operand->str;
				branch->branch->value.siz = left_operand->siz;


				ast_new_branch(branch);
				if (right_operand->nxt) {
					printf("'%s' precedence is %d\n", ast_type_str[branch->type], right_operand->nxt->precedence > operator->precedence);
				}
				if (right_operand->nxt && right_operand->nxt->precedence > operator->precedence) {
					branch->branch->type = AST_SECTION;
					tkn = operator;
				} else {
					if (right_operand->type == TKN_IDENTIFIER) {
						branch->branch->type = AST_IDENTIFIER;
					} else if (right_operand->type == TKN_INTEGER) {
						branch->branch->type = AST_INTEGER;
					} else {
						fprintf(stderr, "ERROR: operand '%.*s' not handled\n", right_operand->siz, right_operand->str); 
						exit(1);
					}
					branch->branch->value.str = right_operand->str;
					branch->branch->value.siz = right_operand->siz;
					tkn = right_operand;
				}

				if (right_operand->nxt && right_operand->nxt->precedence > operator->precedence) {
					branch = branch->branch;
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

ast_t *
statements_to_ast() {
	ast_t *root = arena_alloc(sizeof(ast_t));
	root->type = AST_PROGRAM;
	root->branch = NULL;
	root->hbranch = NULL;
	root->nxt = NULL;
	root->prv = NULL;
	root->precedence = 0;

	root->stat = NULL;
	root->hstat = NULL;
	int count;
	do {
		root->stat = tokens_to_statements(root->stat);
		if (root->stat) print_statement(root->stat);
		if (!root->hstat && root->stat) root->hstat = root->stat;
		count++;
	} while(root->stat);

	root->stat = root->hstat;
	token_t *tkn = root->stat->htkn;
	ast_new_branch(root);
	ast_t *branch = root->branch;
	ast_t *left_operand = NULL;


	// TODO: semicolon error handling
	while (root->stat) {
		switch(segment) {
		case SEG_LOGIC:
			switch(tkn->type) {
			case TKN_SEGMENT: change_segment(tkn); break;
			case TKN_KEYWORD:
				branch->type = AST_RETURN;
				if (tkn->nxt) {
					ast_new_branch(branch);
					branch->branch->type = AST_SECTION;
					branch = branch->branch;
				}
				break;
			case TKN_INTEGER:
				tkn=ast_handle_operator_with_type(AST_INTEGER, &branch, tkn);
				break;
			case TKN_IDENTIFIER:
				tkn=ast_handle_operator_with_type(AST_IDENTIFIER, &branch, tkn);
				break;
			case TKN_OPERATOR:
				if (!tkn->nxt) {
					fprintf(stderr, "ERROR: '%.*s' without a right operand\n", tkn->siz, tkn->str);
					exit(1);
				}
				while (branch->root) {
					if (branch->root->precedence >= branch->precedence) break;
					left_operand = branch;
					branch = branch->root;
				}
				ast_new_branch(branch);
				branch = branch->branch;
				token_operator_to_ast_operator(tkn, branch);
				ast_branch_change_root(left_operand, branch);
				ast_new_branch(branch);
				if (tkn->nxt->type == TKN_INTEGER) {
					branch->branch->type = AST_INTEGER;
				} else if (tkn->nxt->type == TKN_IDENTIFIER) {
					branch->branch->type = AST_IDENTIFIER;
				} else {
					fprintf(stderr, "ERROR: '%.*s' is not handled in a operation\n", tkn->nxt->siz, tkn->nxt->str); 
					exit(1);
				}
				branch->branch->value.siz = tkn->nxt->siz;
				branch->branch->value.str = tkn->nxt->str;
				tkn = tkn->nxt;
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
				branch->branch->type = AST_TYPE;
				branch->branch->value.str = tkn->str;
				branch->branch->value.siz = tkn->siz;

				tkn = tkn->nxt;

				ast_new_branch(branch);
				branch->branch->type = AST_IDENTIFIER;
				branch->branch->value.str = tkn->str;
				branch->branch->value.siz = tkn->siz;
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
			int is_segment = root->stat->htkn->type == TKN_SEGMENT;
			root->stat = root->stat->nxt;
			if (!root->stat) continue;
			tkn = root->stat->htkn;
			if (is_segment) continue;
			ast_new_branch(root);
			branch = root->branch;
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
		printf("%s : %.*s\n", ast_type_str[root->type], root->value.siz, root->value.str);
	}
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
	src = arena_alloc(f_siz);
	fread(src, f_siz, 1, f);
	fclose(f);

	ast_t *ast = statements_to_ast();

	print_ast(ast, 0);

	arena_destroy();
	return 0;
}

