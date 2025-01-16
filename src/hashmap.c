#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

/* tool functions */
static uint8_t hash(char *key)
{
	uint8_t val = 0, i;
	for (; *key; ++key) {
		val = (val << 1) + *key;
		if ((i = val & ~HASHMAP_BITS))
			val = (val ^ (i >> 6)) & HASHMAP_BITS;
	}
	return val;
}

static struct _hashmap_item_t *hashmap_item_new(char *key, void *value)
{
	struct _hashmap_item_t *new = malloc(sizeof(*new) + strlen(key) + 1);
	new->next = NULL;
	new->value = value;
	strcpy(new->key, key);

	return new;
}

/* public defn.s */
hashmap_t hashmap_new(void)
{
	struct _hashmap_t *new = calloc(1, sizeof(*new));
	return new;
}

void *hashmap_find(hashmap_t map, char *key)
{
	uint8_t i = hash(key);
	struct _hashmap_item_t *item = map->data[i];
	if (!item)
		return NULL;
	
	do {
		if (strcmp(key, item->key) == 0)
			return item->value;
	} while ((item = item->next));

	return NULL;
}

void hashmap_upsert(hashmap_t map, char *key, void *value)
{
	uint8_t i = hash(key);
	struct _hashmap_item_t *item = map->data[i];
	if (!item) {
		map->data[i] = hashmap_item_new(key, value);
		return;
	}

	while (item) {
		if (strcmp(key, item->key) == 0) {
			item->value = value;
			return;
		}
		item = item->next;
	}
	item = hashmap_item_new(key, value);

	return;
}

void hashmap_destroy(hashmap_t map)
{
	for (size_t i = 0; i < HASHMAP_SIZE; ++i) {
		struct _hashmap_item_t *item = map->data[i];
		struct _hashmap_item_t *next;
		while (item) {
			next = item->next;
			free(item);
			item = next;
		}
	}
	free(map);
}
