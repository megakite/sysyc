#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "memir.h"
#include "types.h"

/* state variables */
static koopa_raw_basic_block_t g_curr_basic_block;
static koopa_raw_value_t g_curr_value;

/* accessor decl.s */
static koopa_raw_program_t memir_CompUnit(struct node_t *node);
static koopa_raw_function_t memir_FuncDef(struct node_t *node);
static koopa_raw_type_t memir_FuncType(struct node_t *node);
static koopa_raw_basic_block_t memir_Block(struct node_t *node);
static koopa_raw_value_t memir_Stmt(struct node_t *node);
static koopa_raw_value_t memir_Number(struct node_t *node);

/* tool functions */
static koopa_raw_slice_t make_slice(uint32_t len,
				    koopa_raw_slice_item_kind_t kind)
{
	koopa_raw_slice_t slice = { .len = len, .kind = kind, };

	if (len == 0)
		slice.buffer = NULL;
	else
		slice.buffer = malloc(sizeof(void *) * len);

	return slice;
}

/* accessor defn.s */
static koopa_raw_type_t memir_FuncType(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_FuncType);

	char *type = node->_children[0]->data.ast_data.value.s;

	koopa_raw_type_kind_t *ret = malloc(sizeof(*ret));
	if (strcmp(type, "int") == 0) {
		koopa_raw_type_kind_t *function_ret =
			malloc(sizeof(*function_ret));
		function_ret->tag = KOOPA_RTT_INT32;

		ret->tag = KOOPA_RTT_FUNCTION;
		ret->data.function.params = make_slice(0, KOOPA_RSIK_TYPE);
		ret->data.function.ret = function_ret;
	} else {
		assert(false /* unknown FuncType */);
	}

	return ret;
}

static koopa_raw_basic_block_t memir_Block(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Block);

	koopa_raw_basic_block_data_t *ret = malloc(sizeof(*ret));
	g_curr_basic_block = ret;

	ret->name = strdup("\%entry");
	ret->params = make_slice(0, KOOPA_RSIK_VALUE);
	ret->used_by = make_slice(0, KOOPA_RSIK_VALUE);
	ret->insts = make_slice(1, KOOPA_RSIK_VALUE);
	ret->insts.buffer[0] = memir_Stmt(node->_children[0]);

	return ret;
}

static koopa_raw_value_t memir_Number(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Number);

	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = make_slice(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = g_curr_value;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = node->_children[0]->data.ast_data.value.i,
	};

	return ret;
}

static koopa_raw_value_t memir_Stmt(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_Stmt);

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	g_curr_value = ret;

	switch (node->_children[0]->data.ast_data.kind) {
	case AST_RETURN: {
		koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
		ret_ty->tag = KOOPA_RTT_UNIT;

		ret->ty = ret_ty;
		ret->name = NULL;
		ret->used_by = make_slice(0, KOOPA_RSIK_VALUE);
		ret->kind = (koopa_raw_value_kind_t) {
			.tag = KOOPA_RVT_RETURN,
			.data.ret.value = memir_Number(node->_children[1]),
		};
	}
		break;
	default:
		assert(false /* unimplemented */);
	}

	return ret;
}

static koopa_raw_function_t memir_FuncDef(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_FuncDef);

	char name[IDENT_MAX + 1];
	snprintf(name, IDENT_MAX + 1, "@%s",
		 node->_children[1]->data.ast_data.value.s);
	name[IDENT_MAX] = '\0';

	koopa_raw_function_data_t *ret = malloc(sizeof(*ret));
	ret->ty = memir_FuncType(node->_children[0]);
	ret->name = strdup(name);
	ret->params = make_slice(0, KOOPA_RSIK_VALUE);
	ret->bbs = make_slice(1, KOOPA_RSIK_BASIC_BLOCK);
	ret->bbs.buffer[0] = memir_Block(node->_children[2]);

	return ret;
}

static koopa_raw_program_t memir_CompUnit(struct node_t *node)
{
	assert(node);
	assert(node->data.ast_data.kind == AST_CompUnit);

	koopa_raw_program_t ret = {
		.values = make_slice(0, KOOPA_RSIK_VALUE),
		.funcs = make_slice(1, KOOPA_RSIK_FUNCTION),
	};
	ret.funcs.buffer[0] = memir_FuncDef(node->_children[0]);

	return ret;
}

/* public defn.s */
inline koopa_raw_program_t memir_gen(struct node_t *program)
{
	return memir_CompUnit(program);
}
