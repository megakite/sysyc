#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "koopaext.h"
#include "memir.h"
#include "types.h"

/* state variables */
vector_t g_vec_functions;
vector_t g_vec_values;

static koopa_raw_basic_block_t g_curr_basic_block;

/* accessor decl.s */
static koopa_raw_program_t memir_CompUnit(struct node_t *node);
static koopa_raw_function_t memir_FuncDef(struct node_t *node);
static koopa_raw_type_t memir_FuncType(struct node_t *node);
static koopa_raw_basic_block_t memir_Block(struct node_t *node);

static koopa_raw_value_t memir_Stmt(struct node_t *node);
static koopa_raw_value_t memir_Number(struct node_t *node,
				      koopa_raw_value_t used_by);
static koopa_raw_value_t memir_Exp(struct node_t *node,
				   koopa_raw_value_t used_by);
static koopa_raw_value_t memir_UnaryExp(struct node_t *node,
					koopa_raw_value_t used_by);
static koopa_raw_value_t memir_PrimaryExp(struct node_t *node,
					  koopa_raw_value_t used_by);

/* tool functions */
static koopa_raw_value_t value_integer_new(int32_t value,
					   koopa_raw_value_t used_by)
{
	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = value,
	};

	return ret;
}

static koopa_raw_value_data_t *value_binary_new(koopa_raw_binary_op_t op,
						koopa_raw_value_t lhs,
						koopa_raw_value_t rhs,
						koopa_raw_value_t used_by)
{
	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_BINARY,
		.data.binary = { .op = op, .lhs = lhs, .rhs = rhs, },
	};

	return ret;
}

/* accessor defn.s */
static koopa_raw_type_t memir_FuncType(struct node_t *node)
{
	assert(node);
	assert(node->data.ast.kind == AST_FuncType);

	char *type = node->children[0]->data.ast.value.s;

	koopa_raw_type_kind_t *ret = malloc(sizeof(*ret));
	if (strcmp(type, "int") == 0)
	{
		koopa_raw_type_kind_t *function_ret =
			malloc(sizeof(*function_ret));
		function_ret->tag = KOOPA_RTT_INT32;

		ret->tag = KOOPA_RTT_FUNCTION;
		ret->data.function.params = slice_new(0, KOOPA_RSIK_TYPE);
		ret->data.function.ret = function_ret;
	}
	else
	{
		assert(false /* unknown FuncType */);
	}

	return ret;
}

static koopa_raw_basic_block_t memir_Block(struct node_t *node)
{
	assert(node);
	assert(node->data.ast.kind == AST_Block);

	koopa_raw_basic_block_data_t *ret = malloc(sizeof(*ret));
	g_curr_basic_block = ret;

	ret->name = strdup("\%entry");
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->insts = slice_new(0, KOOPA_RSIK_VALUE);

	memir_Stmt(node->children[0]);

	return ret;
}

static koopa_raw_value_t memir_PrimaryExp(struct node_t *node,
					  koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.ast.kind == AST_PrimaryExp);

	/* sole Number, propagate */
	if (node->children[0]->data.ast.kind == AST_Number)
		return memir_Number(node->children[0], used_by);
	/* compound Exp, propagate */
	if (node->children[0]->data.ast.kind == AST_Exp)
		return memir_Exp(node->children[0], used_by);

	assert(false /* unreachable */);
}

static koopa_raw_value_t memir_UnaryExp(struct node_t *node,
					koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.ast.kind == AST_UnaryExp);

	/* sole primary expression (fixed point) */
	if (node->size == 1)
		return memir_PrimaryExp(node->children[0], used_by);

	/* otherwise, continguous unary expression */
	char op_token = node->children[0]->data.ast.value.s[0];

	/* unary plus; basically does nothing, propagate */
	if (op_token == '+')
		return memir_UnaryExp(node->children[1], used_by);

	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind.tag = KOOPA_RVT_BINARY;
	ret->kind.data.binary = (koopa_raw_binary_t) {
		.lhs = value_integer_new(0, ret),
		.rhs = memir_UnaryExp(node->children[1], ret),
	};

	switch (op_token)
	{
	case '-':
		ret->kind.data.binary.op = KOOPA_RBO_SUB;
		break;
	case '!':
		ret->kind.data.binary.op = KOOPA_RBO_EQ;
		break;
	default:
		assert(false /* wrong operator */);
	}

	slice_append(&g_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t memir_Exp(struct node_t *node,
				   koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.ast.kind == AST_Exp);

	/* unary expression, propagate */
	if (node->size == 1)
		return memir_UnaryExp(node->children[0], used_by);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.ast.value.s;
	struct node_t *lhs = node->children[0];
	struct node_t *rhs = node->children[2];

	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind.tag = KOOPA_RVT_BINARY;

	/* logical operators are kinda naughty */
	if (node->children[1]->data.ast.kind != AST_LOR &&
	    node->children[1]->data.ast.kind != AST_LAND)
	{
		ret->kind.data.binary.lhs = memir_Exp(lhs, ret);
		ret->kind.data.binary.rhs = memir_Exp(rhs, ret);
	}

	koopa_raw_value_data_t *insert;
	switch (node->children[1]->data.ast.kind)
	{
	case AST_LOR:
		ret->kind.data.binary.op = KOOPA_RBO_OR;

		/* insert another two predicates to determine logical truth */
		insert = value_binary_new(KOOPA_RBO_NOT_EQ, NULL, NULL, ret);
		insert->kind.data.binary.lhs = value_integer_new(0, insert);
		insert->kind.data.binary.rhs = memir_Exp(lhs, insert);
		slice_append(&g_curr_basic_block->insts, insert);
		ret->kind.data.binary.lhs = insert;

		insert = value_binary_new(KOOPA_RBO_NOT_EQ, NULL, NULL, ret);
		insert->kind.data.binary.lhs = value_integer_new(0, insert);
		insert->kind.data.binary.rhs = memir_Exp(rhs, insert);
		slice_append(&g_curr_basic_block->insts, insert);
		ret->kind.data.binary.rhs = insert;
		break;
	case AST_LAND:
		ret->kind.data.binary.op = KOOPA_RBO_OR;

		/* insert another two predicates to determine logical truth */
		insert = value_binary_new(KOOPA_RBO_NOT_EQ, NULL, NULL, ret);
		insert->kind.data.binary.lhs = value_integer_new(0, insert);
		insert->kind.data.binary.rhs = memir_Exp(lhs, insert);
		slice_append(&g_curr_basic_block->insts, insert);
		ret->kind.data.binary.lhs = insert;

		insert = value_binary_new(KOOPA_RBO_NOT_EQ, NULL, NULL, ret);
		insert->kind.data.binary.lhs = value_integer_new(0, insert);
		insert->kind.data.binary.rhs = memir_Exp(rhs, insert);
		slice_append(&g_curr_basic_block->insts, insert);
		ret->kind.data.binary.rhs = insert;
		break;
	case AST_EQOP:
		if (strcmp(op_token, "!=") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_NOT_EQ;
		else if (strcmp(op_token, "==") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_EQ;
		else
			assert(false /* unsupported EQOP */);
		break;
	case AST_RELOP:
		if (strcmp(op_token, ">") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_GT;
		else if (strcmp(op_token, "<") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_LT;
		else if (strcmp(op_token, ">=") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_GE;
		else if (strcmp(op_token, "<=") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_LE;
		else
			assert(false /* unsupported RELOP */);
		break;
	case AST_ADDOP:
		if (strcmp(op_token, "+") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_ADD;
		else if (strcmp(op_token, "-") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_SUB;
		else
			assert(false /* unsupported ADDOP */);
		break;
	case AST_MULOP:
		if (strcmp(op_token, "*") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_MUL;
		else if (strcmp(op_token, "/") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_DIV;
		else if (strcmp(op_token, "%") == 0)
			ret->kind.data.binary.op = KOOPA_RBO_MOD;
		else
			assert(false /* unsupported MULOP */);
		break;
	default:
		assert(false /* todo */);
	}

	slice_append(&g_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t memir_Number(struct node_t *node,
				      koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.ast.kind == AST_Number);

	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = node->children[0]->data.ast.value.i,
	};

	return ret;
}

static koopa_raw_value_t memir_Stmt(struct node_t *node)
{
	assert(node);
	assert(node->data.ast.kind == AST_Stmt);

	koopa_raw_type_kind_t *ret_ty = malloc(sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_UNIT;

	koopa_raw_value_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_values, ret);
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);

	switch (node->children[0]->data.ast.kind)
	{
	case AST_RETURN:
		ret->kind = (koopa_raw_value_kind_t) {
			.tag = KOOPA_RVT_RETURN,
			.data.ret.value = memir_Exp(node->children[1], ret),
		};
		break;
	default:
		assert(false /* todo */);
	}

	slice_append(&g_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_function_t memir_FuncDef(struct node_t *node)
{
	assert(node);
	assert(node->data.ast.kind == AST_FuncDef);

	char name[IDENT_MAX + 1];
	snprintf(name, IDENT_MAX + 1, "@%s",
		 node->children[1]->data.ast.value.s);
	name[IDENT_MAX] = '\0';

	koopa_raw_function_data_t *ret = malloc(sizeof(*ret));
	vector_push(g_vec_functions, ret);
	ret->ty = memir_FuncType(node->children[0]);
	ret->name = strdup(name);
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->bbs = slice_new(1, KOOPA_RSIK_BASIC_BLOCK);
	ret->bbs.buffer[0] = memir_Block(node->children[2]);

	return ret;
}

static koopa_raw_program_t memir_CompUnit(struct node_t *node)
{
	assert(node);
	assert(node->data.ast.kind == AST_CompUnit);

	koopa_raw_program_t ret = {
		.values = slice_new(0, KOOPA_RSIK_VALUE),
		.funcs = slice_new(1, KOOPA_RSIK_FUNCTION),
	};
	ret.funcs.buffer[0] = memir_FuncDef(node->children[0]);

	return ret;
}

/* public defn.s */
inline koopa_raw_program_t memir_gen(struct node_t *program)
{
	g_vec_functions = vector_new(0);
	g_vec_values = vector_new(0);

	return memir_CompUnit(program);
}
