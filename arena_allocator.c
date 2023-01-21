#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <stddef.h>

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

enum {
	ARENA_MEMORY = 0,
	ARENA_BLOCKS,
	ARENA_COUNT,
};

static struct heap {
	arena_t *arenas[ARENA_COUNT];
	arena_t *harenas[ARENA_COUNT];

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
heap_grow(size_t amount, int arena) {
	amount = align_to(HEAP_ALIGN, amount) + sizeof(arena_t);
	void *limit = sbrk(0);
	if (heap.limit && heap.limit != limit) {
		fprintf(stderr, "ERROR: couldn't grow the heap break point\n");
		exit(1);
	}
	if (!heap.limit) heap.memory = sbrk(0);
	sbrk(amount);
	heap.limit = sbrk(0);

	arena_t *prv = heap.arenas[arena];
	heap.arenas[arena] = (void *)heap.memory + heap.size;
	heap.arenas[arena]->memory = (void *)heap.arenas[arena] + sizeof(arena_t);
	heap.arenas[arena]->offset = 0;
	heap.arenas[arena]->size = amount - sizeof(arena_t);
	heap.arenas[arena]->nxt = NULL;
	if (!heap.harenas[arena]) heap.harenas[arena] = heap.arenas[arena];
	else							prv->nxt = heap.arenas[arena];
	heap.size += amount;
}

memory_block_t *
heap_find_free_block(size_t size) {
	arena_t *blocks = heap.harenas[ARENA_BLOCKS];
	while (blocks) {
		for (size_t i = 0; i < blocks->offset; i++)
			if (!blocks->block[i].deleted && blocks->block[i].free && blocks->block[i].size >= size) return blocks->block + i;
		blocks = blocks->nxt;
	}
	return NULL;
}

memory_block_t *
heap_create_block() {
	arena_t *blocks = heap.harenas[ARENA_BLOCKS];
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
	heap_grow(1, ARENA_BLOCKS);
	blocks = heap.arenas[ARENA_BLOCKS];
	blocks->block[blocks->offset].free = 1;
	blocks->block[blocks->offset].deleted = 0;
	return blocks->block + blocks->offset++;
}

void *
malloc(size_t amount) {
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
	if (!heap.arenas[ARENA_MEMORY] || heap.arenas[ARENA_MEMORY]->offset + amount > heap.arenas[ARENA_MEMORY]->size) heap_grow(amount, ARENA_MEMORY);
	memory_block_t *new_block = heap_create_block();
	new_block->arena = heap.arenas[ARENA_MEMORY];
	new_block->offset = heap.arenas[ARENA_MEMORY]->offset;
	new_block->size = amount;
	new_block->free = 0;
	heap.arenas[ARENA_MEMORY]->offset += amount;
	return new_block->arena->memory + new_block->offset;
}

memory_block_t *
heap_find_block(void *memory) {
	arena_t *blocks = heap.harenas[ARENA_BLOCKS];
	while (blocks) {
		for (unsigned int i = 0; i < blocks->offset; i++) {
			printf("blocks->block %lu\n", blocks->offset);
			if (!blocks->block[i].deleted && !blocks->block[i].free && blocks->block[i].arena->memory + blocks->block[i].offset == memory) {
				return blocks->block + i;
			}
		}
		blocks = blocks->nxt;
	}
	return NULL;
}

void
free(void *memory) {
	memory_block_t *block = heap_find_block(memory);
	printf("%p\n", block->free);
	block->free = 1;
}

void *
realloc(void *memory, size_t new_size) {
	(void)memory;
	(void)new_size;
	assert(0 && "not implemented");
}

void
clean_up() {
	void *limit = sbrk(0);
	if (heap.limit && heap.limit == limit) {
		sbrk(-heap.size);
		heap = (struct heap){0};
	}
}

int
main(void) {
	void *p1 = malloc(50);
	printf("%lu\n", (ptrdiff_t)p1);
	void *p2 = malloc(50);
	printf("%lu\n", (ptrdiff_t)p2);
	free(p2);
	clean_up();
	return 0;
}
