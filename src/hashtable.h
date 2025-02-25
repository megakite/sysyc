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

/* HashTable<Ptr, UInt32> */
typedef struct _htable_ptru32_t *htable_ptru32_t;

htable_ptru32_t htable_ptru32_new(void);
void htable_ptru32_delete(htable_ptru32_t table);

uint32_t *htable_ptru32_lookup(const htable_ptru32_t table, void *key);
uint32_t *htable_ptru32_insert(htable_ptru32_t table, void *key,
			       uint32_t value);
void htable_ptru32_iterate(htable_ptru32_t table,
			   void (*fn)(void **, uint32_t *));

/* HashTable<String, Symbol> */
typedef struct _htable_strsym_t *htable_strsym_t;

htable_strsym_t htable_strsym_new(void);
void htable_strsym_delete(htable_strsym_t table);

struct view_t htable_strsym_lookup(const htable_strsym_t table, char *key);
struct symbol_t *htable_strsym_insert(htable_strsym_t table, char *key,
				      struct symbol_t value);
void htable_strsym_iterate(htable_strsym_t table,
			   void (*fn)(char **, struct symbol_t *));

/* HashTable<...Args, Ptr> */
typedef struct _htable_argptr_t *htable_argptr_t;

htable_argptr_t htable_argptr_new(void);
void htable_argptr_delete(htable_argptr_t table);

struct view_t htable_argptr_lookup(const htable_argptr_t table, char *key);
struct symbol_t *htable_argptr_insert(htable_argptr_t table, char *key,
				      struct symbol_t value);
void htable_argptr_iterate(htable_argptr_t table,
			   void (*fn)(char **, struct symbol_t *));

#define htable_lookup(table, key) _Generic((table),	\
		htable_strsym_t: htable_strsym_lookup,	\
		htable_ptru32_t: htable_ptru32_lookup,	\
		htable_argptr_t: htable_argptr_lookup	\
	)(table, key)

#define htable_insert(table, key, value) _Generic((table),	\
		htable_strsym_t: htable_strsym_insert,		\
		htable_ptru32_t: htable_ptru32_insert,		\
		htable_argptr_t: htable_argptr_insert		\
	)(table, key, value)

#define htable_iterate(table, fn) _Generic((table),	\
		htable_strsym_t: htable_strsym_iterate,	\
		htable_ptru32_t: htable_ptru32_iterate,	\
		htable_argptr_t: htable_argptr_iterate	\
	)(table, fn)

#endif//_HASHTABLE_H_
