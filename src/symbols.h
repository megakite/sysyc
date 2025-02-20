/**
 * symbols.h
 * A very, very, very lame symbol table.
 */

#ifndef _SYMTABLE_H_
#define _SYMTABLE_H_

#include <stdbool.h>

#include "koopa.h"

enum symbol_tag_e {
	CONSTANT = 0,
	VARIABLE,
	FUNCTION,
};

enum function_type_e {
	VOID = 0,
	INT,
};

struct symbol_meta_t {
	int16_t level;
	int16_t scope;
};

struct symbol_t {
	struct symbol_meta_t meta;
	enum symbol_tag_e tag;
	union {
		struct {
			int32_t value;
			koopa_raw_value_t raw;
		} constant;
		struct {
			koopa_raw_value_t raw;
		} variable;
		struct {
			enum function_type_e type;	
			koopa_raw_function_t raw;
		} function;
	};
};

typedef struct _symbols_t *symbols_t;

/* ctor. dtor. */
symbols_t symbols_new(void);
void symbols_delete(symbols_t symbols);

/* getters */
int16_t symbols_level(const symbols_t symbols);
int16_t symbols_scope(const symbols_t symbols);

/* scope actions */
void symbols_indent(symbols_t symbols);
void symbols_leave(symbols_t symbols);
void symbols_enter(symbols_t symbols);
void symbols_dedent(symbols_t symbols);

/* symbol operations */
bool symbols_here(const symbols_t symbols, struct symbol_t *symbol);
bool symbols_saw(const symbols_t symbols, struct symbol_t *symbol);
void symbols_add(symbols_t symbols, char *key, struct symbol_t value);
struct view_t symbols_lookup(const symbols_t symbols, char *key);
struct symbol_t *symbols_get(const symbols_t symbols, char *ident);

#endif//_SYMTABLE_H_
