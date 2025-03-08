/**
 * symbols.c
 * Implementation of symbol table used in this compiler.
 */

/**
 * MEMORY          +-----+-----+-----+---------+-----+
 * LAYOUT   levels | [0] | [1] | ... | [k - 1] | [k] |<--[level]
 *         (stack) +--|--+--|--+-----+--|------+--|--+
 *                    V     V           V         V
 *                 +-----+-----+     +-----+   +-----+
 *          scopes | [0] | [0]-. ... | [0] |   | [0] |
 *         (queue) +--\--+-----+\    +--/--+   +--|--+
 *                     \ | [1] | \     /          V
 *                      \+--|--+  \   /         .link = NULL (just a block)
 *                       \  |      \ /
 *            .last       '-|-------/-----------.
 *              |           |      / \           \
 *              |   .-------------'   \           \
 * table        |  /        |          \           \
 * (hash table) V V         V           V           V
 * +-----+  +--------+  +--------+  +--------+  +--------+
 * | [0]--->| symbol |->| symbol |->| symbol |->| global |
 * +-----+  +--------+  +--------+  +--------+  +--------+
 * | [1] |                                      |  link  |
 * +-----+                                      +---/----+
 * | ... |                                         /
 * +-----+  +--------+                            /
 * | [N]--->| global |<--------------------------'
 * +-----+  +--------+
 *
 * i must admit that it's certainly not a good idea at all to implement a
 * _persistent_ symbol table using such complicated combination of hash tables,
 * stacks, and vectors...it must be better to just use an rbtree.
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "globals.h"
#include "hashtable.h"
#include "symbols.h"
#include "vector.h"

/* useful macros */
#define container_of(ptr, type, member)	\
	(type *)((char *)(ptr) - offsetof(type, member))

/* XXX friend class HashTable<String, Symbol> */
struct _htable_strsym_item_t {
	struct _htable_strsym_item_t *next;
	struct _htable_strsym_item_t *link;
	uint8_t hash;
	struct symbol_t value;
	char key[];
};

struct _htable_strsym_t {
	struct _htable_strsym_item_t *data[HASHTABLE_SIZE];
};

/* opaque definition */
struct _symbols_t {
	htable_strsym_t table;
	struct vector_ptr_t *levels;

	/* state variables */
	uint16_t depth;
	int16_t level;
};

/* ctor. of symbols */
struct symbol_t symbol_constant(int32_t value)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = CONSTANT,
		.constant = {
			.value = value,
			.raw = NULL,
		},
	};
	return symbol;
}

struct symbol_t symbol_variable(void)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = VARIABLE,
		.variable = {
			.raw = NULL,
		},
	};
	return symbol;
}

struct symbol_t symbol_function(struct vector_typ_t *params,
				enum symbol_type_e type)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = FUNCTION,
		.function = {
			.raw = NULL,
			.params = /* move */ params,
			.type = type,
		},
	};
	return symbol;
}

struct pair_t {
	struct _htable_strsym_item_t *link;
	struct _htable_strsym_item_t *last;
};

/* private class Layer; basically a queue */
struct level_t {
	uint16_t begin;
	uint16_t end;
	int16_t scope;
	struct pair_t scopes[];
};

static struct level_t *level_new(void)
{
	struct level_t *new = malloc(sizeof(*new));
	new->begin = 0;
	new->end = 0;
	new->scope = -1;
	return new;
}

static void level_delete(struct level_t *level)
{
	free(level);
}

static bool level_empty(const struct level_t *level)
{
	return level->begin == level->end;
}

static struct level_t *level_offer(struct level_t *level,
				   struct _htable_strsym_item_t *item)
{
	level->scope = level->end++;
	struct level_t *new = realloc(level, sizeof(*new) +
				      sizeof(*new->scopes) * level->end);
	new->scopes[new->scope] = (struct pair_t) { .link = item,
						    .last = item, };

	return new;
}

static struct _htable_strsym_item_t *level_poll(struct level_t *level)
{
	if (level_empty(level))
		return NULL;

	return level->scopes[level->begin++].link;
}

static struct _htable_strsym_item_t *level_at(const struct level_t *level)
{
	return level->scopes[level->scope].link;
}

/* exported methods */
symbols_t symbols_new(void)
{
	struct _symbols_t *new = malloc(sizeof(*new));
	new->table = htable_strsym_new();
	new->levels = vector_ptr_new(1);
	new->depth = 0;
	new->level = -1;

	// global level
	symbols_indent(new);

	return new;
}

void symbols_delete(symbols_t symbols)
{
	// global level
	symbols_dedent(symbols);

	vector_ptr_delete(symbols->levels);
	htable_strsym_delete(symbols->table);
	free(symbols);
}

/* property accessors */
int16_t symbols_level(const symbols_t symbols)
{
	return symbols->level;
}

int16_t symbols_scope(const symbols_t symbols)
{
	struct level_t *level = symbols->levels->data[symbols->level];
	return level->scope;
}

/* these two predicates have _a lot_ implications on states... */
bool symbols_here(const symbols_t symbols, struct symbol_t *symbol)
{
	if (symbols->level != symbol->meta.level)
		return false;

	struct level_t *level = symbols->levels->data[symbols->level];
	return level->scope == symbol->meta.scope;
}

bool symbols_saw(const symbols_t symbols, struct symbol_t *symbol)
{
	if (symbols->level < symbol->meta.level)
		return false;

	/* i believe this is both ultimately clever and ultimately hacky, or at
	 * least ultimately...hard to understand. */
	struct level_t *symbol_level =
		symbols->levels->data[symbol->meta.level];
	return symbol_level->scope == symbol->meta.scope;
}

struct symbol_t *symbols_get(const symbols_t symbols, char *ident)
{
	struct symbol_t *symbol;
	struct view_t view = symbols_lookup(symbols, ident);
	while ((symbol = view.next(&view)))
		if (symbols_saw(symbols, symbol))
			break;
	return symbol;
}

struct symbol_t *symbols_add(symbols_t symbols, char *ident,
			     struct symbol_t symbol)
{
	struct level_t *level = symbols->levels->data[symbols->level];
	struct pair_t *scope = &level->scopes[level->scope];
	struct symbol_t *new = htable_insert(symbols->table, ident, symbol);
	struct _htable_strsym_item_t *new_item =
		container_of(new, struct _htable_strsym_item_t, value);

	if (!scope->last)
		scope->link = new_item;
	else
		scope->last->link = new_item;
	scope->last = new_item;

	return new;
}

struct view_t symbols_lookup(const symbols_t symbols, char *ident)
{
	return htable_lookup(symbols->table, ident);
}

/* scope controllers' basic workflow:
 * indent() => leave() => enter() => leave()
 *                     => enter() => leave() 
 *                              ...
 *                     => enter() => leave() => enter() => dedent()
 * |-----buildup-----|    |-----visit------|    |-----destroy-----| */
void symbols_indent(symbols_t symbols)
{
	if (++symbols->level == symbols->depth)
	{
		++symbols->depth;
		vector_ptr_push(symbols->levels, level_new());
	}

 	/* this is what i call "the weird pattern" when we use flexible array
	 * members: we need another level of indirection */
	struct level_t **level =
		(struct level_t **)&symbols->levels->data[symbols->level];
	*level = level_offer(*level, NULL);
}

void symbols_leave(symbols_t symbols)
{
	struct level_t *level;

	/* leave until this block has ended */
	do
		level = symbols->levels->data[symbols->level--];
	while (symbols->level > 0 && level_at(level) != NULL);
}

void symbols_enter(symbols_t symbols)
{
	++symbols->level;

	struct level_t *level = symbols->levels->data[symbols->level];
	level->scope = (level->scope + 1) % level->end;
}

void symbols_dedent(symbols_t symbols)
{
	bool outside = false;
	do
	{
		struct level_t *level = symbols->levels->data[symbols->level--];
		assert(level->scope == level->begin);

		struct _htable_strsym_item_t *link = level_poll(level);
		if (!link)
			outside = true;

		struct _htable_strsym_item_t *this;
		while ((this = link))
		{
			link = this->link;

			/* ugh, it's really weird to not use a head node... */
			struct _htable_strsym_item_t **it =
				&symbols->table->data[this->hash];
			struct _htable_strsym_item_t **prev;
			do
			{
				prev = it;
				it = &(*it)->next;
			}
			while (*prev != this);
			*prev = *it;

			/* FIXME ugly */
			if (this->value.tag == FUNCTION)
				vector_typ_delete(this->value.function.params);
			free(this);
		}

		if (level_empty(level))
			level_delete(level);
	}
	while (symbols->level > 0 && !outside);
}
