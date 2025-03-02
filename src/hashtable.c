#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "macros.h"

/* opaque type definitions */
struct _htable_ptru32_item_t {
	struct _htable_ptru32_item_t *next;
	uint32_t value;
	void *key;
};

struct _htable_ptru32_t {
	struct _htable_ptru32_item_t *data[HASHTABLE_SIZE];
};

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

/* hash functions */
static uint8_t hash_str(char *key)
{
	uint8_t val = 0, i;
	for (; *key; ++key)
	{
		val = (val << 1) + *key;
		if ((i = val & ~HASHTABLE_BITS))
			val = (val ^ (i >> 6)) & HASHTABLE_BITS;
	}
	return val;
}

static uint8_t hash_ptr(void *p)
{
	/* really need `constexpr` here */
	if (sizeof(void *) == 8)
		return (((uintptr_t)p) * 0x9E3779B97F4A7C15) >> 56;
	if (sizeof(void *) == 4)
		return (((uintptr_t)p) * 0x9E3779B9) >> 24;
}

/* tool functions */
static struct _htable_strsym_item_t *htable_strsym_item_new(uint8_t hash,
	char *key, struct symbol_t value)
{
	struct _htable_strsym_item_t *new =
		malloc(sizeof(*new) + strlen(key) + 1);
	new->next = NULL;
	new->link = NULL;
	new->hash = hash;
	new->value = value;
	strcpy(new->key, key);

	return new;
}

static struct _htable_ptru32_item_t *htable_ptru32_item_new(void *key,
							    uint32_t value)
{
	struct _htable_ptru32_item_t *new = malloc(sizeof(*new));
	new->next = NULL;
	new->value = value;
	new->key = key;

	return new;
}

/* HashTable<String, Ptr> */
htable_strsym_t htable_strsym_new(void)
{
	struct _htable_strsym_t *new = calloc(1, sizeof(*new));
	return new;
}

static void *strsym_next(struct view_t *this)
{
	bool found = false;
	struct symbol_t *ret = NULL;
	struct _htable_strsym_item_t *item;
	while (!found
	       && (item = container_of(this->begin,
				       struct _htable_strsym_item_t, value)))
	{
		if (strcmp(item->key, this->key) == 0)
			found = true;

		/* return this->begin++; */
		ret = &item->value;
		this->begin = &item->next->value;
	}

	return ret;
}

struct view_t htable_strsym_lookup(htable_strsym_t table, char *key)
{
	uint8_t i = hash_str(key);
	struct _htable_strsym_item_t *item = table->data[i];
	if (!item)
		return (struct view_t) { .next = &VIEW_NULL };

	return (struct view_t) { .begin = &item->value, .next = &strsym_next,
				 .key = key };
}

struct symbol_t *htable_strsym_insert(htable_strsym_t table, char* key,
				      struct symbol_t value)
{
	uint8_t i = hash_str(key);
	struct _htable_strsym_item_t *new = htable_strsym_item_new(i, key,
								   value);
	if (table->data[i])
		new->next = table->data[i];

	table->data[i] = new;
	return &new->value;
}

void htable_strsym_delete(htable_strsym_t table)
{
	for (size_t i = 0; i < HASHTABLE_SIZE; ++i)
	{
		struct _htable_strsym_item_t *item = table->data[i];
		struct _htable_strsym_item_t *next;
		while (item)
		{
			next = item->next;
			free(item);
			item = next;
		}
	}
	free(table);
}

/* HashTable<Ptr, UInt32> */
htable_ptru32_t htable_ptru32_new(void)
{
	struct _htable_ptru32_t *new = calloc(1, sizeof(*new));
	return new;
}

uint32_t *htable_ptru32_lookup(htable_ptru32_t table, void *key)
{
	uint8_t i = hash_ptr(key);
	struct _htable_ptru32_item_t *item = table->data[i];
	if (!item)
		return NULL;

	do
	{
		if (key == item->key)
			return &item->value;
	} while ((item = item->next));

	return NULL;
}

uint32_t *htable_ptru32_insert(htable_ptru32_t table, void *key, uint32_t value)
{
	uint8_t i = hash_ptr(key);
	struct _htable_ptru32_item_t *new = htable_ptru32_item_new(key, value);
	if (table->data[i])
		new->next = table->data[i];

	table->data[i] = new;
	return &new->value;
}

void htable_ptru32_delete(htable_ptru32_t table)
{
	for (size_t i = 0; i < HASHTABLE_SIZE; ++i)
	{
		struct _htable_ptru32_item_t *item = table->data[i];
		struct _htable_ptru32_item_t *next;
		while (item)
		{
			next = item->next;
			free(item);
			item = next;
		}
	}
	free(table);
}
