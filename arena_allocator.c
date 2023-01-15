#include <stdio.h>
#include <assert.h>
#include <unistd.h>

typedef struct memory_block {
	unsigned int size;
	unsigned int offset;
	struct memory_block *nxt;
	struct memory_block *prv;
	int free;
} memory_block_t;

#define ARENA_PAGE 4096
struct {
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
		arena.cap = ARENA_PAGE;
		arena.limit = sbrk(0);
	} else {
		fprintf(stderr, "ERROR: couldn't grow the arena heap\n");
	}
}

void *
arena_alloc(unsigned int amount) {
	if (amount == 0) amount = 1;
	amount = align_to_ptr(amount);

	printf("(amount %d) ", amount);
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
		printf("offset: %d, block-offset: %d, size: %d, block: %ld\n", arena.offset, free_block->offset, free_block->size, (unsigned char *)free_block - arena.buf);
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
			printf("free: offset: %d, block-offset: %d, block: %ld\n", arena.offset, new_block->offset, (unsigned char *)new_block - arena.buf);
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
	printf("offset: %d, block-offset: %d\n", arena.offset, new_block->offset);
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
		printf("realloc: offset: %d, block-offset: %d, block: %ld\n", arena.offset, new_block->offset, (unsigned char *)new_block - arena.buf);
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
	int *numbers = arena_alloc(sizeof(int) * 4);
	numbers[0] = 10;
	numbers[1] = 20;
	numbers[2] = 30;
	numbers[3] = 40;
	numbers = arena_realloc(numbers, sizeof(int) * 8);
	numbers[4] = 50;
	numbers[5] = 60;
	numbers[6] = 70;
	numbers[7] = 80;

	for (int i = 0; i < 8; i++) {
		printf("numbers[%d] = %d\n", i, numbers[i]);
	}
	
	arena_destroy();
	return 0;
}
