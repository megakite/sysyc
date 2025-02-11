#ifndef _SEMANTIC_H_
#define _SEMANTIC_H_

#include "node.h"
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

struct symbol_t {
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

void semantic(struct node_t *comp_unit);

#endif//_SEMANTIC_H_
