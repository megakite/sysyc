#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ast.h"

struct node_t *ast_nterm(enum ast_kind_e kind, int line, int count, ...)
{
	assert(kind > AST_END_OF_TERM);
	union node_data_u data = {
		.ast_data = {
			.terminal = false,
			.line = line,
			.kind = kind,
		}
	};
	struct node_t *new = node_new(NODE_AST, data, count);

	va_list args;
	va_start(args, count);

	for (int i = 0; i < count; ++i) {
		new = node_add_child(new, va_arg(args, struct node_t *));
	}

	va_end(args);

	return new;
}

struct node_t *_ast_term_int(enum ast_kind_e kind, int value)
{
	assert(kind != AST_UNKNOWN);
	assert(kind < AST_END_OF_TERM);
	union node_data_u data = {
		.ast_data = {
			.terminal = true,
			.kind = kind,
			.value.i = value,
		}
	};

	return node_new(NODE_AST, data, 0);
}

struct node_t *_ast_term_float(enum ast_kind_e kind, float value)
{
	assert(kind != AST_UNKNOWN);
	assert(kind < AST_END_OF_TERM);
	union node_data_u data = {
		.ast_data = {
			.terminal = true,
			.kind = kind,
			.value.f = value,
		}
	};

	return node_new(NODE_AST, data, 0);
}

struct node_t *_ast_term_string(enum ast_kind_e kind, const char *value)
{
	assert(kind != AST_UNKNOWN);
	assert(kind < AST_END_OF_TERM);
	union node_data_u data = {
		.ast_data = {
			.terminal = true,
			.kind = kind,
		}
	};
	strncpy(data.ast_data.value.s, value, IDENT_MAX);
	data.ast_data.value.s[IDENT_MAX - 1] = '\0';

	return node_new(NODE_AST, data, 0);
}

static void _ast_print_lambda(void *ptr, int depth)
{
	struct node_t *node = ptr;

	printf("%p\t", node);
	for (int i = 0; i < depth; ++i) {
		putchar(' ');
	}

	struct ast_data_t *data = &node->data.ast_data;

	switch (data->kind) {
	case AST_UNKNOWN:
		assert(false /* should not have reached here */);
		break;
	case AST_INT_CONST:
		printf("%s : %d\n", AST_KIND_STRINGS[data->kind],
		       data->value.i);
		break;
	case AST_FLOAT:
		printf("%s : %f\n", AST_KIND_STRINGS[data->kind],
		       data->value.f);
		break;
	case AST_IDENT:
	case AST_TYPE:
	case AST_RELOP:
		printf("%s : %s\n", AST_KIND_STRINGS[data->kind],
		       data->value.s);
		break;
	default:
		if (data->terminal) {
			printf("%s\n", AST_KIND_STRINGS[data->kind]);
		} else {
			printf("%s (%d)\n", AST_KIND_STRINGS[data->kind],
			       data->line);
		}
	}
}

inline void ast_print(struct node_t *node)
{
	node_traverse_pre_depth(node, _ast_print_lambda, 0);
}
