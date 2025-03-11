/**
 * hashtable.h
 * Hash table implementation.
 */

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdint.h>

#include "semantic.h"
#include "symbols.h"
#include "view.h"

#define HASHTABLE_BITS 0xff
#define HASHTABLE_SIZE 256

/* HashTable<Pair<Ptr, UInt32>, UInt32> */
struct pair_ptru32_t {
	void *ptr;
	uint32_t u32;
};

#define make_pair(ptr, u32) ((struct pair_ptru32_t) { ptr, u32 })

typedef struct _htable_ppuu32_t *htable_ppuu32_t;

htable_ppuu32_t htable_ppuu32_new(void);
void htable_ppuu32_delete(htable_ppuu32_t table);

uint32_t *htable_ppuu32_lookup(const htable_ppuu32_t table,
			       struct pair_ptru32_t key);
uint32_t *htable_ppuu32_insert(htable_ppuu32_t table, struct pair_ptru32_t key,
			       uint32_t value);

/* HashTable<Ptr, UInt32> */
typedef struct _htable_ptru32_t *htable_ptru32_t;

htable_ptru32_t htable_ptru32_new(void);
void htable_ptru32_delete(htable_ptru32_t table);

uint32_t *htable_ptru32_lookup(const htable_ptru32_t table, void *key);
uint32_t *htable_ptru32_insert(htable_ptru32_t table, void *key,
			       uint32_t value);

/* HashTable<String, Symbol> */
typedef struct _htable_strsym_t *htable_strsym_t;

htable_strsym_t htable_strsym_new(void);
void htable_strsym_delete(htable_strsym_t table);

struct view_t htable_strsym_lookup(const htable_strsym_t table, char *key);
struct symbol_t *htable_strsym_insert(htable_strsym_t table, char *key,
				      struct symbol_t value);

#define htable_lookup(table, key) _Generic((table),	\
		htable_strsym_t: htable_strsym_lookup,	\
		htable_ptru32_t: htable_ptru32_lookup,	\
		htable_ppuu32_t: htable_ppuu32_lookup	\
	)(table, key)

#define htable_insert(table, key, value) _Generic((table),	\
		htable_strsym_t: htable_strsym_insert,		\
		htable_ptru32_t: htable_ptru32_insert,		\
		htable_ppuu32_t: htable_ppuu32_insert		\
	)(table, key, value)

#endif//_HASHTABLE_H_
