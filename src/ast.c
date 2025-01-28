#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ast.h"
#include "types.h"

struct node_t *ast_nterm(enum ast_kind_e kind, int lineno, int count, ...)
{
	assert(kind > AST_END_OF_TERM);

	struct node_data_t data = {
		.tag = NODE_AST,
		.ast = {
			.kind = kind,
			.terminal = false,
			.lineno = lineno,
		}
	};
	struct node_t *new = node_new(data, count);

	va_list args;
	va_start(args, count);

	for (int i = 0; i < count; ++i)
		new = node_add_child(new, va_arg(args, struct node_t *));

	va_end(args);

	return new;
}

struct node_t *_ast_term_int(enum ast_kind_e kind, int value)
{
	assert(kind != AST_UNKNOWN);
	assert(kind < AST_END_OF_TERM);

	struct node_data_t data = {
		.tag = NODE_AST,
		.ast = {
			.terminal = true,
			.kind = kind,
			.value.i = value,
		}
	};

	return node_new(data, 0);
}

struct node_t *_ast_term_string(enum ast_kind_e kind, const char *value)
{
	assert(kind != AST_UNKNOWN);
	assert(kind < AST_END_OF_TERM);

	struct node_data_t data = {
		.tag = NODE_AST,
		.ast = {
			.terminal = true,
			.kind = kind,
		},
	};
	strncpy(data.ast.value.s, value, IDENT_MAX);
	data.ast.value.s[IDENT_MAX - 1] = '\0';

	return node_new(data, 0);
}

static void lambda_print(void *ptr, int depth)
{
	struct node_t *node = ptr;

	for (int i = 0; i < depth * 2; ++i)
		putchar(' ');

	struct ast_data_t *ast = &node->data.ast;

	switch (ast->kind)
	{
	case AST_UNKNOWN:
		assert(false /* unreachable */);
		break;
	case AST_INT_CONST:
		printf("%s : %d\n", AST_KIND_S[ast->kind], ast->value.i);
		break;
	case AST_IDENT:
	case AST_TYPE:
	case AST_RELOP:
	case AST_EQOP:
	case AST_ADDOP:
	case AST_MULOP:
	case AST_UNARYOP:
		printf("%s : %s\n", AST_KIND_S[ast->kind], ast->value.s);
		break;
	default:
		if (ast->terminal)
			printf("%s\n", AST_KIND_S[ast->kind]);
		else
			printf("%s (%d)\n", AST_KIND_S[ast->kind], ast->lineno);
	}
}

inline void ast_print(struct node_t *node)
{
	node_traverse_pre_depth(node, lambda_print, 0);
}
