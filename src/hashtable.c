#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

/* opaque type definitions */
struct _ht_ptru32_item_t {
	struct _ht_ptru32_item_t *next;
	uint32_t value;
	void *key;
};

struct _ht_ptru32_t {
	struct _ht_ptru32_item_t *data[HASHTABLE_SIZE];
};

struct _ht_strsym_item_t {
	struct _ht_strsym_item_t *next;
	struct symbol_t value;
	char key[];
};

struct _ht_strsym_t {
	struct _ht_strsym_item_t *data[HASHTABLE_SIZE];
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

inline static uint8_t hash_ptr(void *p)
{
	/* really need `constexpr` here */
	if (sizeof(void *) == 8)
		return (((uintptr_t)p) * 0x9E3779B97F4A7C15) >> 56;
	if (sizeof(void *) == 4)
		return (((uintptr_t)p) * 0x9E3779B9) >> 24;
}

/* tool functions */
static struct _ht_strsym_item_t *ht_strsym_item_new(char *key,
						    struct symbol_t value)
{
	struct _ht_strsym_item_t *new = malloc(sizeof(*new) + strlen(key) + 1);
	new->next = NULL;
	new->value = value;
	strcpy(new->key, key);

	return new;
}

static struct _ht_ptru32_item_t *ht_ptru32_item_new(void *key, uint32_t value)
{
	struct _ht_ptru32_item_t *new = malloc(sizeof(*new));
	new->next = NULL;
	new->value = value;
	new->key = key;

	return new;
}

/* HashTable<String, Ptr> */
ht_strsym_t ht_strsym_new(void)
{
	struct _ht_strsym_t *new = calloc(1, sizeof(*new));
	return new;
}

struct symbol_t *ht_strsym_find(ht_strsym_t table, char *key)
{
	uint8_t i = hash_str(key);
	struct _ht_strsym_item_t *item = table->data[i];
	if (!item)
		return NULL;

	do
	{
		if (strcmp(key, item->key) == 0)
			return &item->value;
	} while ((item = item->next));

	return NULL;
}

void ht_strsym_upsert(ht_strsym_t table, char *key, struct symbol_t value)
{
	uint8_t i = hash_str(key);
	struct _ht_strsym_item_t *item = table->data[i];
	if (!item)
	{
		table->data[i] = ht_strsym_item_new(key, value);
		return;
	}

	while (item)
	{
		if (strcmp(key, item->key) == 0)
		{
			item->value = value;
			return;
		}
		item = item->next;
	}
	struct _ht_strsym_item_t *new = ht_strsym_item_new(key, value);
	new->next = table->data[i]->next;
	table->data[i] = new;
}

void ht_strsym_delete(ht_strsym_t table)
{
	for (size_t i = 0; i < HASHTABLE_SIZE; ++i)
	{
		struct _ht_strsym_item_t *item = table->data[i];
		struct _ht_strsym_item_t *next;
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
ht_ptru32_t ht_ptru32_new(void)
{
	struct _ht_ptru32_t *new = calloc(1, sizeof(*new));
	return new;
}

uint32_t *ht_ptru32_find(ht_ptru32_t table, void *key)
{
	uint8_t i = hash_ptr(key);
	struct _ht_ptru32_item_t *item = table->data[i];
	if (!item)
		return NULL;

	do
	{
		if (key == item->key)
			return &item->value;
	} while ((item = item->next));

	return NULL;
}

void ht_ptru32_upsert(ht_ptru32_t table, void *key, uint32_t value)
{
	uint8_t i = hash_ptr(key);
	struct _ht_ptru32_item_t *item = table->data[i];
	if (!item)
	{
		table->data[i] = ht_ptru32_item_new(key, value);
		return;
	}

	while (item)
	{
		if (key == item->key)
		{
			item->value = value;
			return;
		}
		item = item->next;
	}
	struct _ht_ptru32_item_t *new = ht_ptru32_item_new(key, value);
	new->next = table->data[i]->next;
	table->data[i] = new;
}

void ht_ptru32_delete(ht_ptru32_t table)
{
	for (size_t i = 0; i < HASHTABLE_SIZE; ++i)
	{
		struct _ht_ptru32_item_t *item = table->data[i];
		struct _ht_ptru32_item_t *next;
		while (item)
		{
			next = item->next;
			free(item);
			item = next;
		}
	}
	free(table);
}
