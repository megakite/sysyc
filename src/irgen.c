#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "irgen.h"

static char *irgen_Number(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Number);

	int int_const = node->_children[0]->data.ast_data.value.i;
	int len = snprintf(NULL, 0, "%d", int_const);
	char *ret = malloc(len + 1);
	sprintf(ret, "%d", int_const);

	return ret;
}

static char *irgen_Stmt(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Stmt);

	int len;
	char *ret;

	char *number = NULL;

	enum ast_kind_e stmt_kind = node->_children[0]->data.ast_data.kind;
	switch (stmt_kind) {
	case AST_RETURN:
		number = irgen_Number(node->_children[1]);
		len = snprintf(NULL, 0, "ret %s\n", number);
		ret = malloc(len + 1);
		sprintf(ret, "ret %s\n", number);
		free(number);
		break;
	default:
		assert(false /* shouldn't have been here */);
	}

	return ret;
}

static char *irgen_FuncType(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_FuncType);

	const char *type = node->_children[0]->data.ast_data.value.s;

	int len = snprintf(NULL, 0, "%s", type);
	char *ret = malloc(len + 1);
	sprintf(ret, "%s", type);

	return ret;
}

static char *irgen_Block(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Block);

	char *stmt = irgen_Stmt(node->_children[0]);

	int len = snprintf(NULL, 0, "%%entry:\n%s\n", stmt);
	char *ret = malloc(len + 1);
	sprintf(ret, "%%entry:\n%s\n", stmt);

	free(stmt);
	return ret;
}

static char *irgen_FuncDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_FuncDef);

	char *func_type_raw = irgen_FuncType(node->_children[0]);

	char *func_type;
	const char *ident = node->_children[1]->data.ast_data.value.s;
	char *block = irgen_Block(node->_children[2]);

	if (strcmp(func_type_raw, "int") == 0) {
		func_type = "i32";
	} else {
		assert(false /* shouldn't have been here */);
	}

	int len = snprintf(NULL, 0, "fun @%s(): %s{\n%s\n}\n", ident, func_type,
			   block);
	char *ret = malloc(len + 1);
	sprintf(ret, "fun @%s(): %s{\n%s\n}\n", ident, func_type, block);

	free(func_type_raw);
	free(block);
	return ret;
}

static char *irgen_CompUnit(const struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_CompUnit);

	char *func_def = irgen_FuncDef(node->_children[0]);

	int len = snprintf(NULL, 0, "%s\n", func_def);
	char *ret = malloc(len + 1);
	sprintf(ret, "%s\n", func_def);

	free(func_def);
	return ret;
}

/* public defn.s */
char *irgen(const struct node_t *node)
{
	if (!node)
		return NULL;

	return irgen_CompUnit(node);
}
