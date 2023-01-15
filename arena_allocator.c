#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct memory_block {
	unsigned int size;
	unsigned int offset;
	struct memory_block *nxt;
	struct memory_block *prv;
	int free;
} memory_block_t;

#define ARENA_CAP 1024
typedef struct arena {
	unsigned char buf[ARENA_CAP];
	unsigned int offset;
	memory_block_t block_buf[ARENA_CAP];
	memory_block_t *block;
	memory_block_t *hblock; // header block
	memory_block_t *ublock; // unused block
	memory_block_t *hublock; // header unused block
	unsigned int block_buf_offset;
	unsigned int id;
	struct arena *nxt;
} arena_t;

static struct {
	arena_t *cur;
	arena_t *head;
} arenas = {0};

#define BLOCK_IN_BYTES align_to_ptr(sizeof(memory_block_t))

memory_block_t *
arena_find_free(arena_t *arena, unsigned int amount) {
	memory_block_t *block = NULL;
	block = arena->hblock;
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

void *
arena_alloc(unsigned int amount) {
	if (amount == 0) amount = 1;
	amount = align_to_ptr(amount);

	printf("(amount %d) ", amount);
	void *memory = NULL;
	arena_t *arena = arenas.head;
	while(!memory) {
		if (amount + BLOCK_IN_BYTES > ARENA_CAP) break;
		if (!arena) {
			arena = malloc(sizeof(arena_t));
			arena->nxt = NULL;
			arena->offset = 0;
			arena->block = NULL;
			arena->hblock = NULL;
			arena->ublock = NULL;
			arena->hublock = NULL;
			if (arenas.cur) {
				arenas.cur->nxt = arena;
				arena->id = arenas.cur->id + 1;
			}
			arenas.cur = arena;
			if (!arenas.head) {
				arena->id = 0;
				arenas.head = arena;
			}
		}

		memory_block_t *free_block = arena_find_free(arena, amount);
		if (free_block) {
			memory = arena->buf + free_block->offset;
			unsigned int diff = free_block->size - amount;
			printf("arena: %d, offset: %d, block-offset: %d, size: %d, block: %ld\n", arena->id, arena->offset, free_block->offset, free_block->size, (unsigned char *)free_block - arena->buf);
			free_block->size -= diff;
			free_block->free = 0;
			if (diff != 0) {
				memory_block_t *new_block = (memory_block_t *)(arena->buf + arena->offset);
				arena->offset += BLOCK_IN_BYTES;
				new_block->free = 1;
				new_block->size = diff;
				new_block->offset = free_block->offset + free_block->size;
				new_block->prv = free_block;
				new_block->nxt = free_block->nxt;
				free_block->nxt = new_block;
				if (!new_block->nxt) {
					arena->block = new_block;
				}
				printf("free: arena: %d, offset: %d, block-offset: %d, block: %ld\n", arena->id, arena->offset, new_block->offset, (unsigned char *)new_block - arena->buf);
			}
			break;
		}
		if (arena->offset + amount + BLOCK_IN_BYTES > ARENA_CAP) {
			arena = arena->nxt;
			continue;
		} else {
			//TODO: allocate memory_block in the arena instead of using malloc
			memory_block_t *new_block = (memory_block_t *)(arena->buf + arena->offset);
			arena->offset += BLOCK_IN_BYTES;
			new_block->size = amount;
			memory = arena->buf + arena->offset;
			new_block->offset = arena->offset;
			new_block->free = 0;
			new_block->nxt = NULL;
			new_block->prv = arena->block;
			if (arena->block) arena->block->nxt = new_block;
			arena->block = new_block;
			if (!arena->hblock) arena->hblock = new_block;
			arena->offset += amount;
			printf("arena: %d, offset: %d, block-offset: %d\n", arena->id, arena->offset, new_block->offset);
		}
	}
	if (!memory) {
		fprintf(stderr, "ERROR: requesting too much memory\n");
	}
	return memory;
}

int
arena_free(void *memory) {
	if (memory == NULL) goto arena_free_invalid;
	arena_t *arena = arenas.head;
	memory_block_t *block = NULL;
	while (arena) {
		if ((void *)arena->buf > memory || memory > (void *)arena->buf + ARENA_CAP) {
			arena = arena->nxt;
			continue;
		}
		block = arena->hblock;
		while(arena->buf + block->offset != memory) {
			block = block->nxt;
			if (block == NULL) goto arena_free_invalid;
		}
		if (block->free) goto arena_free_invalid;
		block->free = 1;
		if (block->nxt && block->nxt->free) {
			memory_block_t *del_block = block->nxt;
			block->nxt = del_block->nxt;
			block->nxt->prv = block;
			if ((void *)del_block==arena->buf + block->offset + block->size) {
				block->size += del_block->size + BLOCK_IN_BYTES;
			} else {
				block->size += del_block->size;
				del_block->nxt = NULL;
				if (!arena->hublock) {
					del_block->prv = NULL;
					arena->hublock = del_block;
					arena->ublock  = del_block;
				} else {
					arena->ublock->nxt = del_block;
				}
			}
		}
		if (block->prv && block->prv->free) {
			memory_block_t *del_block = block;
			block = block->prv;

			if ((void *)del_block==arena->buf + block->offset + block->size) {
				block->size += del_block->size + BLOCK_IN_BYTES;
				block->nxt = del_block->nxt;
			} else {
				block = del_block;
			}
		}
		memory_block_t *ublock = arena->hublock;
		while(ublock) {
			if ((void *)ublock == arena->buf + block->offset + block->size) {
				if (ublock->prv) ublock->prv->nxt = ublock->nxt;
				else {
					arena->hublock = ublock->nxt;
					arena->ublock = ublock->nxt;
				}
				block->size += BLOCK_IN_BYTES;
			}
			if ((void *)ublock == arena->buf + block->offset -BLOCK_IN_BYTES){
				if (ublock->prv) ublock->prv->nxt = ublock->nxt;
				else {
					arena->hublock = ublock->nxt;
					arena->ublock = ublock->nxt;
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
					arena->hblock = new_block;
					arena->block  = new_block;
				}
			}
			ublock = ublock->nxt;
		}
		break;
	}
	return 0;
arena_free_invalid:
	fprintf(stderr, "ERROR: trying to free invalid memory\n");
	return 1;
}

void
arena_destroy(void) {
	while (arenas.head) {
		arenas.cur = arenas.head;
		arenas.head = arenas.head->nxt;
		free(arenas.cur);
	}
}

typedef struct {
	int a; // 4
	int b; // 8
	int c; // 12
	int d; // 16
	int e; // 20
	int f; // 24
} test_t;

int
main(void) { 
	//(OOOO)O|(OOOO)O
	//(OOOO)0|(OOOO)O
	//(OOOO)0|(OOOO)0
	//(OOOO)000000
	//(OOOO)OOO|000|<-(OOOO)
	//(OOOO)000|000|<-(OOOO)
	//(OOOO)000000|[0000]|(OOOO)OOOO
	//(OOOO)000000|[0000]|(OOOO)0000
	//(OOOO)000000|(OOOO)00000000

	int *ptr1 = arena_alloc(sizeof(int));
	printf("%p\n", ptr1);
	int *ptr2 = arena_alloc(sizeof(int));
	printf("%p\n", ptr2);
	arena_free(ptr1);
	arena_free(ptr2);
	int *ptr3 = arena_alloc(sizeof(test_t));
	printf("%p\n", ptr3);
	int *ptr4 = arena_alloc(32);
	printf("%p\n", ptr4);
	arena_free(ptr3);
	arena_free(ptr4);

	arena_destroy();
	return 0;
}
