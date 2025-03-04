/**
 * ast.h
 * Utilities for AST nodes.
 */

#ifndef _AST_H_
#define _AST_H_

#include <stdarg.h>

#include "node.h"

struct node_t *ast_nterm(enum ast_kind_e kind, int count, ...);

struct node_t *_ast_term_int(enum ast_kind_e kind, int value);
struct node_t *_ast_term_string(enum ast_kind_e kind, const char *value);

#define ast_term(kind, value) _Generic(((kind), (value)),	\
		int: _ast_term_int,				\
		char *: _ast_term_string			\
	)((kind), (value))

void ast_print(struct node_t *node);

#endif//_AST_H_
