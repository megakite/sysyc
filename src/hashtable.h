#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdint.h>

#include "semantic.h"

#define HASHTABLE_BITS 0xff
#define HASHTABLE_SIZE 256

/* HashTable<Ptr, UInt32> */
typedef struct _ht_ptru32_t *ht_ptru32_t;

ht_ptru32_t ht_ptru32_new(void);
void ht_ptru32_delete(ht_ptru32_t table);

uint32_t *ht_ptru32_find(ht_ptru32_t table, void *key);
void ht_ptru32_upsert(ht_ptru32_t table, void *key, uint32_t value);
void ht_ptru32_iterate(ht_ptru32_t table, void (*fn)(void *, uint32_t));

/* HashTable<String, Symbol> */
typedef struct _ht_strsym_t *ht_strsym_t;

ht_strsym_t ht_strsym_new(void);
void ht_strsym_delete(ht_strsym_t table);

struct symbol_t *ht_strsym_find(ht_strsym_t table, char *key);
void ht_strsym_upsert(ht_strsym_t table, char *key, struct symbol_t value);
void ht_strsym_iterate(ht_strsym_t table, void (*fn)(char *, struct symbol_t));

#define ht_find(table, key) _Generic((table),	\
		ht_strsym_t: ht_strsym_find,	\
		ht_ptru32_t: ht_ptru32_find	\
	)(table, key)

#define ht_upsert(table, key, value) _Generic((table),	\
		ht_strsym_t: ht_strsym_upsert,		\
		ht_ptru32_t: ht_ptru32_upsert		\
	)(table, key, value)

#define ht_iterate(table, fn) _Generic((table),	\
		ht_strsym_t: ht_strsym_iterate,	\
		ht_ptru32_t: ht_ptru32_iterate	\
	)(table, fn)

#endif//_HASHTABLE_H_
