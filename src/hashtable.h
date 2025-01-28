#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdint.h>

#define HASHTABLE_BITS 0xff
#define HASHTABLE_SIZE 256

/* HashTable<Ptr, UInt32> */
struct _ht_ptru32_item_t {
	struct _ht_ptru32_item_t *next;
	uint32_t value;
	void *key;
};

struct _ht_ptru32_t {
	struct _ht_ptru32_item_t *data[HASHTABLE_SIZE];
};

typedef struct _ht_ptru32_t *ht_ptru32_t;

ht_ptru32_t ht_ptru32_new(void);
void ht_ptru32_delete(ht_ptru32_t table);

uint32_t *ht_ptru32_find(ht_ptru32_t table, void *key);
void ht_ptru32_upsert(ht_ptru32_t table, void *key, uint32_t value);
void ht_ptru32_iterate(ht_ptru32_t table, void (*fn)(void *, uint32_t));

/* HashTable<String, Ptr> */
struct _ht_strptr_item_t {
	struct _ht_strptr_item_t *next;
	void *value;
	char key[];
};

struct _ht_strptr_t {
	struct _ht_strptr_item_t *data[HASHTABLE_SIZE];
};

typedef struct _ht_strptr_t *ht_strptr_t;

ht_strptr_t ht_strptr_new(void);
void ht_strptr_delete(ht_strptr_t table);

void **ht_strptr_find(ht_strptr_t table, char *key);
void ht_strptr_upsert(ht_strptr_t table, char *key, void *value);
void ht_strptr_iterate(ht_strptr_t table, void (*fn)(char *, void *));

#define ht_find(table, key) _Generic((table),	\
		ht_strptr_t: ht_strptr_find,	\
		ht_ptru32_t: ht_ptru32_find	\
	)(table, key)

#define ht_upsert(table, key, value) _Generic((table),	\
		ht_strptr_t: ht_strptr_upsert,		\
		ht_ptru32_t: ht_ptru32_upsert		\
	)(table, key, value)

#define ht_iterate(table, fn) _Generic((table),	\
		ht_strptr_t: ht_strptr_iterate,	\
		ht_ptru32_t: ht_ptru32_iterate	\
	)(table, fn)

#endif//_HASHTABLE_H_
