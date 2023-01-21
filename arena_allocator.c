#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>

#define HEAP_ALIGN		4096
#define POINTER_SIZE	sizeof(void *)

typedef struct memory_block memory_block_t;

typedef struct arena {
	union {
		char *memory;
		memory_block_t *block;
	};
	size_t offset;
	size_t size;
	struct arena *nxt;
} arena_t;

typedef struct memory_block {
	arena_t *arena;
	size_t offset;
	size_t size;
	int free;
	int deleted;
} memory_block_t;

static struct heap {
	arena_t *arena;
	arena_t *harena;

	arena_t *blocks;
	arena_t *hblocks;

	char *memory;
	size_t size;
	void *limit;
} heap;


size_t
align_to(size_t align, size_t amount) {
	unsigned rest = amount & (align - 1); /*'amount % align' but faster*/
	if (!rest) {
		amount += align - rest;
	}
	return amount;
}

void
heap_grow(size_t amount, int grow_blocks) {
	amount = align_to(HEAP_ALIGN, amount) + sizeof(arena_t);
	void *limit = sbrk(0);
	if (heap.limit && heap.limit != limit) {
		fprintf(stderr, "ERROR: couldn't grow the heap break point\n");
		exit(1);
	}
	if (!heap.limit) heap.memory = sbrk(0);
	sbrk(amount);
	heap.limit = sbrk(0);

	if (!grow_blocks) {
		arena_t *prv = heap.arena;
		heap.arena = (void *)heap.memory + heap.size;
		heap.arena->memory = (void *)heap.arena + sizeof(arena_t);
		heap.arena->offset = 0;
		heap.arena->size = amount - sizeof(arena_t);
		heap.arena->nxt = NULL;
		if (!heap.harena) heap.harena = heap.arena;
		else							prv->nxt = heap.arena;
	} else {
		arena_t *prv = heap.blocks;
		heap.blocks = (void *)heap.memory + heap.size;
		heap.blocks->memory = (void *)heap.blocks + sizeof(arena_t);
		heap.blocks->offset = 0;
		heap.blocks->size = (amount - sizeof(arena_t)) / sizeof(memory_block_t);
		heap.blocks->nxt = NULL;
		if (!heap.hblocks) heap.hblocks = heap.blocks;
		else							prv->nxt = heap.blocks;
	}
	heap.size += amount;
}

memory_block_t *
heap_find_free_block(size_t size) {
	arena_t *blocks = heap.hblocks;
	while (blocks) {
		for (size_t i = 0; i < blocks->offset; i++)
			if (!blocks->block[i].deleted && blocks->block[i].free && blocks->block[i].size >= size) return blocks->block + i;
		blocks = blocks->nxt;
	}
	return NULL;
}

memory_block_t *
heap_create_block() {
	arena_t *blocks = heap.hblocks;
	while (blocks) {
		for (size_t i = 0; i < blocks->offset; i++)
			if (blocks->block[i].deleted) return blocks->block + i;
		if (blocks->offset < blocks->size) {
			blocks->block[blocks->offset].free = 1;
			blocks->block[blocks->offset].deleted = 0;
			return blocks->block + blocks->offset++;
		}
		blocks = blocks->nxt;
	}
	heap_grow(1, 1);
	blocks = heap.blocks;
	blocks->block[blocks->offset].free = 1;
	blocks->block[blocks->offset].deleted = 0;
	return blocks->block + blocks->offset++;
}

void *
heap_alloc(size_t amount) {
	if (!amount) amount = 1;
	amount = align_to(POINTER_SIZE, amount);
	memory_block_t *free_block = heap_find_free_block(amount);
	if (free_block) {
		free_block->free = 0;
		size_t diff = free_block->size - amount;
		free_block->size = amount;
		if (diff) {
			memory_block_t *new_block = heap_create_block();
			new_block->arena = free_block->arena;
			new_block->offset = free_block->offset + free_block->size;
			new_block->size = diff;
		}
		return free_block->arena->memory + free_block->offset;
	}
	if (!heap.arena || heap.arena->offset + amount > heap.arena->size) heap_grow(amount, 0);
	memory_block_t *new_block = heap_create_block();
	new_block->arena = heap.arena;
	new_block->offset = heap.arena->offset;
	new_block->size = amount;
	new_block->free = 0;
	heap.arena->offset += amount;
	return new_block->arena->memory + new_block->offset;
}

void
heap_free(void *memory) {
	(void)memory;
	assert(0 && "not implemented");
}

void *
heap_realloc(void *memory, size_t new_size) {
	(void)memory;
	(void)new_size;
	assert(0 && "not implemented");
}

void
heap_cleanup() {
	void *limit = sbrk(0);
	if (heap.limit && heap.limit == limit) {
		sbrk(-heap.size);
		heap = (struct heap){0};
	}
}

int
main(void) {
	int *numbers1 = heap_alloc(sizeof(int) * 4);
	numbers1[0] = 10;
	numbers1[1] = 15;
	numbers1[2] = 20;
	numbers1[3] = 25;
	int *numbers2 = heap_alloc(sizeof(int) * 4);
	numbers2[0] = 30;
	numbers2[1] = 35;
	numbers2[2] = 40;
	numbers2[3] = 45;
	for (int i = 0; i < 4; i++) {
		printf("numbers1[%d] = %d\n", i, numbers1[i]);
	}
	printf("\n");
	for (int i = 0; i < 4; i++) {
		printf("numbers2[%d] = %d\n", i, numbers2[i]);
	}
	heap_cleanup();
	return 0;
}
