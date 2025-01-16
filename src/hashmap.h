#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <stdbool.h>

#define HASHMAP_BITS 0xff
#define HASHMAP_SIZE 256

struct _hashmap_item_t {
	struct _hashmap_item_t *next;
	void *value;
	char key[];
};

struct _hashmap_t {
	struct _hashmap_item_t *data[HASHMAP_SIZE];
};

typedef struct _hashmap_t *hashmap_t;

hashmap_t hashmap_new(void);
void *hashmap_find(hashmap_t map, char *key);
void hashmap_upsert(hashmap_t map, char *key, void *value);
void hashmap_destroy(hashmap_t map);

#endif//_HASHMAP_H_
