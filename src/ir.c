#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "koopaext.h"
#include "ir.h"
#include "ast.h"

/* state variables */
static koopa_raw_basic_block_t m_curr_basic_block;
static koopa_raw_type_kind_t *m_curr_function_ret;

/* accessor decl.s */
static koopa_raw_program_t ir_CompUnit(struct node_t *node);
static koopa_raw_function_t ir_FuncDef(struct node_t *node);
static koopa_raw_type_t ir_FuncType(struct node_t *node);
static koopa_raw_basic_block_t ir_Block(struct node_t *node);

static koopa_raw_value_t ir_Stmt(struct node_t *node);
static koopa_raw_value_t ir_Number(struct node_t *node,
				   koopa_raw_value_t used_by);
static koopa_raw_value_t ir_Exp(struct node_t *node,
				koopa_raw_value_t used_by);
static koopa_raw_value_t ir_UnaryExp(struct node_t *node,
				     koopa_raw_value_t used_by);
static koopa_raw_value_t ir_PrimaryExp(struct node_t *node,
				       koopa_raw_value_t used_by);

static void ir_Decl(struct node_t *node);
#if 0
/* already evaluated during semantic analysis phase */
static koopa_raw_value_t ir_ConstDecl(struct node_t *node);
static koopa_raw_value_t ir_BType(struct node_t *node);
static koopa_raw_value_t ir_ConstDef(struct node_t *node);
static koopa_raw_value_t ir_ConstInitVal(struct node_t *node);
#endif
static void ir_VarDecl(struct node_t *node);
static koopa_raw_value_t ir_VarDef(struct node_t *node);
static koopa_raw_value_t ir_InitVal(struct node_t *node,
				    koopa_raw_value_t used_by);
static void ir_BlockItem(struct node_t *node);
static koopa_raw_value_t ir_LVal(struct node_t *node,
				 koopa_raw_value_t used_by);
#if 0
/* already evaluated during semantic analysis phase */
static koopa_raw_value_t ir_ConstExp(struct node_t *node);
static koopa_raw_value_t ir_ConstDefList(struct node_t *node);
#endif
static void ir_VarDefList(struct node_t *node);
static void ir_BlockItemList(struct node_t *node);

/* tool functions */
static char *name_global_new(char *ident)
{
	if (!ident)
		return NULL;

	char name[IDENT_MAX + 1];
	snprintf(name, IDENT_MAX + 1, "@%s", ident);
	name[IDENT_MAX] = '\0';

	return bump_strdup(g_bump, name);
}

static char *name_local_new(char *ident)
{
	if (!ident)
		return NULL;

	char name[IDENT_MAX + 1];
	snprintf(name, IDENT_MAX + 1, "%%%s", ident);
	name[IDENT_MAX] = '\0';

	return bump_strdup(g_bump, name);
}

static koopa_raw_value_t value_integer_new(char *ident, int32_t value,
					   koopa_raw_value_t used_by)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = name_local_new(ident);
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = value,
	};

	return ret;
}

static koopa_raw_value_t value_alloc_new(char *ident)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_POINTER;
	ret_ty->data.pointer.base = m_curr_function_ret;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = name_global_new(ident);
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_ALLOC,
	};

	return ret;
}

static koopa_raw_value_t value_binary_new(koopa_raw_binary_op_t op,
					  koopa_raw_value_t used_by)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_BINARY,
		.data.binary = { .op = op, .lhs = NULL, .rhs = NULL, },
	};

	return ret;
}

/* accessor defn.s */
static koopa_raw_type_t ir_FuncType(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncType);

	char *type = node->children[0]->data.value.s;

	koopa_raw_type_kind_t *ret = bump_malloc(g_bump, sizeof(*ret));
	if (strcmp(type, "int") == 0)
	{
		koopa_raw_type_kind_t *function_ret =
			bump_malloc(g_bump, sizeof(*function_ret));
		m_curr_function_ret = function_ret;
		function_ret->tag = KOOPA_RTT_INT32;

		ret->tag = KOOPA_RTT_FUNCTION;
		ret->data.function.params = slice_new(0, KOOPA_RSIK_TYPE);
		ret->data.function.ret = function_ret;
	}
	else
		assert(false /* unknown FuncType */);

	return ret;
}

static koopa_raw_basic_block_t ir_Block(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Block);

	koopa_raw_basic_block_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	m_curr_basic_block = ret;

	ret->name = name_local_new("entry");
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->insts = slice_new(0, KOOPA_RSIK_VALUE);

	ir_BlockItemList(node->children[0]);

	return ret;
}

static koopa_raw_value_t ir_PrimaryExp(struct node_t *node,
				       koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_PrimaryExp);

	switch (node->children[0]->data.kind)
	{
	case AST_Number:
		return ir_Number(node->children[0], used_by);
	case AST_Exp:
		return ir_Exp(node->children[0], used_by);
	case AST_LVal:
		{
		koopa_raw_value_t lval = ir_LVal(node->children[0], used_by);
		/* if lval is a constant */
		if (lval->ty->tag == KOOPA_RTT_INT32)
			return lval;

		/* otherwise, insert `load` here instead of in `LVal`, since the
		 * latter one is also used in assignments */
		koopa_raw_type_kind_t *ret_ty =
			bump_malloc(g_bump, sizeof(*ret_ty));
		ret_ty->tag = KOOPA_RTT_INT32;

		koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
		ret->ty = ret_ty;
		ret->name = NULL;
		ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
		ret->used_by.buffer[0] = used_by;
		ret->kind = (koopa_raw_value_kind_t) {
			.tag = KOOPA_RVT_LOAD,
			.data.load.src = ir_LVal(node->children[0], ret),
		};

		slice_append(&m_curr_basic_block->insts, ret);

		return ret;
		}
	default:
		assert(false /* todo */);
	}
}

static koopa_raw_value_t ir_UnaryExp(struct node_t *node,
				     koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_UnaryExp);

	/* sole primary expression (fixed point) */
	if (node->size == 1)
		return ir_PrimaryExp(node->children[0], used_by);

	/* otherwise, continguous unary expression */
	char op_token = node->children[0]->data.value.s[0];

	/* unary plus; basically does nothing, propagate */
	if (op_token == '+')
		return ir_UnaryExp(node->children[1], used_by);

	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind.tag = KOOPA_RVT_BINARY;
	ret->kind.data.binary = (koopa_raw_binary_t) {
		.lhs = value_integer_new(NULL, 0, ret),
		.rhs = ir_UnaryExp(node->children[1], ret),
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

	slice_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t ir_Exp(struct node_t *node,
				koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_Exp);

	/* unary expression, propagate */
	if (node->size == 1)
		return ir_UnaryExp(node->children[0], used_by);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.value.s;
	struct node_t *lhs = node->children[0];
	struct node_t *rhs = node->children[2];

	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind.tag = KOOPA_RVT_BINARY;

	/* logical operators are kinda naughty */
	if (node->children[1]->data.kind != AST_LOR &&
	    node->children[1]->data.kind != AST_LAND)
	{
		ret->kind.data.binary.lhs = ir_Exp(lhs, ret);
		ret->kind.data.binary.rhs = ir_Exp(rhs, ret);
	}

	koopa_raw_value_data_t *insert;
	switch (node->children[1]->data.kind)
	{
	case AST_LOR:
		ret->kind.data.binary.op = KOOPA_RBO_OR;

		/* insert another two predicates to determine logical truth */
		insert = value_binary_new(KOOPA_RBO_NOT_EQ, ret);
		insert->kind.data.binary.lhs = value_integer_new(NULL, 0,
								 insert);
		insert->kind.data.binary.rhs = ir_Exp(lhs, insert);
		slice_append(&m_curr_basic_block->insts, insert);
		ret->kind.data.binary.lhs = insert;

		insert = value_binary_new(KOOPA_RBO_NOT_EQ, ret);
		insert->kind.data.binary.lhs = value_integer_new(NULL, 0,
								 insert);
		insert->kind.data.binary.rhs = ir_Exp(rhs, insert);
		slice_append(&m_curr_basic_block->insts, insert);
		ret->kind.data.binary.rhs = insert;
		break;
	case AST_LAND:
		ret->kind.data.binary.op = KOOPA_RBO_OR;

		/* insert another two predicates to determine logical truth */
		insert = value_binary_new(KOOPA_RBO_NOT_EQ, ret);
		insert->kind.data.binary.lhs = value_integer_new(NULL, 0,
								 insert);
		insert->kind.data.binary.rhs = ir_Exp(lhs, insert);
		slice_append(&m_curr_basic_block->insts, insert);
		ret->kind.data.binary.lhs = insert;

		insert = value_binary_new(KOOPA_RBO_NOT_EQ, ret);
		insert->kind.data.binary.lhs = value_integer_new(NULL, 0,
								 insert);
		insert->kind.data.binary.rhs = ir_Exp(rhs, insert);
		slice_append(&m_curr_basic_block->insts, insert);
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

	slice_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t ir_Number(struct node_t *node,
				   koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_Number);

	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(1, KOOPA_RSIK_VALUE);
	ret->used_by.buffer[0] = used_by;
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = node->children[0]->data.value.i,
	};

	return ret;
}

static koopa_raw_value_t ir_Stmt(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Stmt);

	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_UNIT;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);

	switch (node->children[0]->data.kind)
	{
	case AST_LVal:
		ret->kind = (koopa_raw_value_kind_t) {
			.tag = KOOPA_RVT_STORE,
			.data.store = {
				.value = ir_Exp(node->children[1], ret),
				.dest = ir_LVal(node->children[0], ret),
			},
		};
		break;
	case AST_RETURN:
		ret->kind = (koopa_raw_value_kind_t) {
			.tag = KOOPA_RVT_RETURN,
			.data.ret.value = ir_Exp(node->children[1], ret),
		};
		break;
	default:
		assert(false /* todo */);
	}

	slice_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_function_t ir_FuncDef(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncDef);

	koopa_raw_function_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ir_FuncType(node->children[0]);
	ret->name = name_global_new(node->children[1]->data.value.s);
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->bbs = slice_new(1, KOOPA_RSIK_BASIC_BLOCK);
	ret->bbs.buffer[0] = ir_Block(node->children[2]);

	return ret;
}

static koopa_raw_program_t ir_CompUnit(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_CompUnit);

	koopa_raw_program_t ret = {
		.values = slice_new(0, KOOPA_RSIK_VALUE),
		.funcs = slice_new(1, KOOPA_RSIK_FUNCTION),
	};
	ret.funcs.buffer[0] = ir_FuncDef(node->children[0]);

	return ret;
}

static void ir_Decl(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Decl);

	if (node->children[0]->data.kind == AST_VarDecl)
		ir_VarDecl(node->children[0]);
}

static void ir_VarDecl(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDecl);

	ir_VarDefList(node->children[1]);
}

static koopa_raw_value_t ir_VarDef(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDef);

	char *ident = node->children[0]->data.value.s;
	struct symbol_t *symbol = ht_find(g_symbols, ident);
	assert(symbol);

	koopa_raw_value_data_t *ret = symbol->variable.raw =
		value_alloc_new(ident);

	slice_append(&m_curr_basic_block->insts, ret);

	/* initialized */
	if (node->size == 2)
		slice_append(&m_curr_basic_block->insts,
			     ir_InitVal(node->children[1], ret));

	return ret;
}

static koopa_raw_value_t ir_InitVal(struct node_t *node,
				    koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_InitVal);

	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_UNIT;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_STORE,
		.data.store = {
			.value = ir_Exp(node->children[0], ret),
			.dest = used_by,
		},
	};

	/* very interesting reverse insertion... could be more useful */
	slice_append(&used_by->used_by, ret);

	return ret;
}

static void ir_BlockItem(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItem);

	if (node->children[0]->data.kind == AST_Decl)
		ir_Decl(node->children[0]);
	if (node->children[0]->data.kind == AST_Stmt)
		ir_Stmt(node->children[0]);
}

static koopa_raw_value_t ir_LVal(struct node_t *node,
				 koopa_raw_value_t used_by)
{
	assert(node);
	assert(node->data.kind == AST_LVal);

	char *ident = node->children[0]->data.value.s;
	struct symbol_t *symbol = ht_find(g_symbols, ident);
	assert(symbol);

	koopa_raw_value_data_t *ret;
	switch (symbol->tag)
	{
	case CONSTANT:
		if ((ret = symbol->constant.raw))
			slice_append(&symbol->constant.raw->used_by, used_by);
		else
			ret = symbol->constant.raw =
				value_integer_new(ident, symbol->constant.value,
						  used_by);
		break;
	case VARIABLE:
		assert(ret = symbol->variable.raw);
		slice_append(&symbol->variable.raw->used_by, used_by);
		break;
	default:
		assert(false /* unreachable */);
	}

	return ret;
}

static void ir_VarDefList(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDefList);

	for (int i = node->size - 1; i >= 0; --i)
		ir_VarDef(node->children[i]);
}

static void ir_BlockItemList(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItemList);

	for (int i = node->size - 1; i >= 0; --i)
		ir_BlockItem(node->children[i]);
}

/* public defn.s */
inline koopa_raw_program_t ir(struct node_t *program)
{
	koopa_raw_program_t ret = ir_CompUnit(program);
	ht_strsym_delete(g_symbols);

	return ret;
}
