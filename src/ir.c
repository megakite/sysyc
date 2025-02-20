#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "koopaext.h"
#include "ir.h"
#include "ast.h"
#include "macros.h"
#include "globals.h"

/* state variables */
static koopa_raw_basic_block_t m_curr_basic_block;
static koopa_raw_type_kind_t *m_curr_function_ret;

/* accessor decl.s */
static koopa_raw_program_t CompUnit(const struct node_t *node);
static koopa_raw_function_t FuncDef(const struct node_t *node);
static koopa_raw_type_t FuncType(const struct node_t *node);
static void Block(const struct node_t *node);

static void Stmt(const struct node_t *node);
static koopa_raw_value_t Number(const struct node_t *node);
static koopa_raw_value_t Exp(const struct node_t *node);
static koopa_raw_value_t UnaryExp(const struct node_t *node);
static koopa_raw_value_t PrimaryExp(const struct node_t *node);

static void Decl(const struct node_t *node);
#if 0
/* already evaluated during semantic analysis phase */
static koopa_raw_value_t ConstDecl(const struct node_t *node);
static koopa_raw_value_t BType(const struct node_t *node);
static koopa_raw_value_t ConstDef(const struct node_t *node);
static koopa_raw_value_t ConstInitVal(const struct node_t *node);
#endif
static void VarDecl(const struct node_t *node);
static koopa_raw_value_t VarDef(const struct node_t *node);
static koopa_raw_value_t InitVal(const struct node_t *node);
static void BlockItem(const struct node_t *node);
static koopa_raw_value_t LVal(const struct node_t *node);
#if 0
/* already evaluated during semantic analysis phase */
static koopa_raw_value_t ConstExp(const struct node_t *node);
static koopa_raw_value_t ConstDefList(const struct node_t *node);
#endif
static void VarDefList(const struct node_t *node);
static void BlockItemList(const struct node_t *node);

/* tool functions */
static char *name_global(char *ident)
{
	if (!ident)
		return NULL;

	char name[IDENT_MAX + 1];
	snprintf(name, IDENT_MAX + 1, "@%s", ident);
	name[IDENT_MAX] = '\0';

	return bump_strdup(g_bump, name);
}

static char *name_local(char *ident)
{
	if (!ident)
		return NULL;

#define MANGLED_MAX (1 + IDENT_MAX + (1 + 6) * 2)
	char name[MANGLED_MAX];
	snprintf(name, MANGLED_MAX, "%%%s_%hd_%hd", ident,
		 symbols_level(g_symbols), symbols_scope(g_symbols));
	name[MANGLED_MAX - 1] = '\0';
#undef MANGLED_MAX

	return bump_strdup(g_bump, name);
}

/* accessor defn.s */
static koopa_raw_type_t FuncType(const struct node_t *node)
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
		panic("unsupported FuncType");

	return ret;
}

static void Block(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Block);

	symbols_enter(g_symbols);
	BlockItemList(node->children[0]);
	symbols_dedent(g_symbols);
}

static koopa_raw_value_t PrimaryExp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_PrimaryExp);

	switch (node->children[0]->data.kind)
	{
	case AST_Number:
		return Number(node->children[0]);
	case AST_Exp:
		return Exp(node->children[0]);
	case AST_LVal:
		{
		koopa_raw_value_t lval = LVal(node->children[0]);
		/* if lval is a constant */
		if (lval->ty->tag == KOOPA_RTT_INT32)
			return lval;

		/* otherwise, insert `raw_load` here instead of in `LVal`, since
		 * the latter one is also used in assignments */
		koopa_raw_value_t ret = koopa_raw_load(lval);
		slice_append(&m_curr_basic_block->insts, ret);
		return ret;
		}
	default:
		todo();
	}
}

static koopa_raw_value_t UnaryExp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_UnaryExp);

	/* sole primary expression (fixed point) */
	if (node->size == 1)
		return PrimaryExp(node->children[0]);

	/* otherwise, continguous unary expression */
	char op_token = node->children[0]->data.value.s[0];

	/* unary plus; basically does nothing, propagate */
	if (op_token == '+')
		return UnaryExp(node->children[1]);

	koopa_raw_value_t lhs = koopa_raw_integer(0);
	koopa_raw_value_t rhs = UnaryExp(node->children[1]);
	koopa_raw_value_t ret;
	switch (op_token)
	{
	case '-':
		ret = koopa_raw_binary(KOOPA_RBO_SUB, lhs, rhs);
		break;
	case '!':
		ret = koopa_raw_binary(KOOPA_RBO_EQ, lhs, rhs);
		break;
	default:
		panic("unsupported unary operator");
	}

	slice_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t Exp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Exp);

	/* unary expression, propagate */
	if (node->size == 1)
		return UnaryExp(node->children[0]);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.value.s;
	koopa_raw_value_t lhs = Exp(node->children[0]);
	koopa_raw_value_t rhs = Exp(node->children[2]);

	koopa_raw_value_t ret, ltrue, rtrue;
	switch (node->children[1]->data.kind)
	{
	case AST_LOR:
		/* insert another two predicates to determine logical truth */
		ltrue = koopa_raw_binary(KOOPA_RBO_NOT_EQ, koopa_raw_integer(0),
					 lhs);
		rtrue = koopa_raw_binary(KOOPA_RBO_NOT_EQ, koopa_raw_integer(0),
					 rhs);
		slice_append(&m_curr_basic_block->insts, ltrue);
		slice_append(&m_curr_basic_block->insts, rtrue);

		ret = koopa_raw_binary(KOOPA_RBO_OR, ltrue, rtrue);
		break;
	case AST_LAND:
		/* vice versa */
		ltrue = koopa_raw_binary(KOOPA_RBO_NOT_EQ, koopa_raw_integer(0),
					 lhs);
		rtrue = koopa_raw_binary(KOOPA_RBO_NOT_EQ, koopa_raw_integer(0),
					 rhs);
		slice_append(&m_curr_basic_block->insts, ltrue);
		slice_append(&m_curr_basic_block->insts, rtrue);

		ret = koopa_raw_binary(KOOPA_RBO_AND, ltrue, rtrue);
		break;
	case AST_EQOP:
		if (strcmp(op_token, "!=") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_NOT_EQ, lhs, rhs);
		else if (strcmp(op_token, "==") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_EQ, lhs, rhs);
		else
			panic("unsupported EQOP");
		break;
	case AST_RELOP:
		if (strcmp(op_token, ">") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_GT, lhs, rhs);
		else if (strcmp(op_token, "<") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_LT, lhs, rhs);
		else if (strcmp(op_token, ">=") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_GE, lhs, rhs);
		else if (strcmp(op_token, "<=") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_LE, lhs, rhs);
		else
			panic("unsupported RELOP");
		break;
	case AST_ADDOP:
		if (strcmp(op_token, "+") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_ADD, lhs, rhs);
		else if (strcmp(op_token, "-") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_SUB, lhs, rhs);
		else
			panic("unsupported ADDOP");
		break;
	case AST_MULOP:
		if (strcmp(op_token, "*") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_MUL, lhs, rhs);
		else if (strcmp(op_token, "/") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_DIV, lhs, rhs);
		else if (strcmp(op_token, "%") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_MOD, lhs, rhs);
		else
			panic("unsupported MULOP");
		break;
	default:
		todo();
	}

	slice_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t Number(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Number);

	return koopa_raw_integer(node->children[0]->data.value.i);
}

static void Stmt(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Stmt);

	koopa_raw_value_t inst;
	switch (node->children[0]->data.kind)
	{
	case AST_SEMI:
		/* empty Stmt, do nothing */
		break;
	case AST_LVal:
		inst = koopa_raw_store(Exp(node->children[1]),
				       LVal(node->children[0]));

		slice_append(&m_curr_basic_block->insts, inst);
		break;
	case AST_Exp:
		Exp(node->children[0]);
		break;
	case AST_Block:
		Block(node->children[0]);
		break;
	case AST_RETURN:
		/* RETURN [Exp] SEMI */
		if (node->size == 2)
			inst = koopa_raw_return(Exp(node->children[1]));
		else
			inst = koopa_raw_return(NULL);

		slice_append(&m_curr_basic_block->insts, inst);
		break;
	default:
		todo();
	}
}

static koopa_raw_function_t FuncDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncDef);

	koopa_raw_basic_block_data_t *bb = bump_malloc(g_bump, sizeof(*bb));
	m_curr_basic_block = bb;
	bb->name = name_local("entry");
	bb->params = slice_new(0, KOOPA_RSIK_VALUE);
	bb->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	bb->insts = slice_new(0, KOOPA_RSIK_VALUE);

	koopa_raw_function_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = FuncType(node->children[0]);
	ret->name = name_global(node->children[1]->data.value.s);
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->bbs = slice_new(1, KOOPA_RSIK_BASIC_BLOCK);
	ret->bbs.buffer[0] = bb;

	Block(node->children[2]);

	return ret;
}

static koopa_raw_program_t CompUnit(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_CompUnit);

	koopa_raw_program_t ret = {
		.values = slice_new(0, KOOPA_RSIK_VALUE),
		.funcs = slice_new(1, KOOPA_RSIK_FUNCTION),
	};

	symbols_enter(g_symbols);
	ret.funcs.buffer[0] = FuncDef(node->children[0]);
	symbols_dedent(g_symbols);

	return ret;
}

static void Decl(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Decl);

	symbols_enter(g_symbols);
	if (node->children[0]->data.kind == AST_VarDecl)
		VarDecl(node->children[0]);
}

static void VarDecl(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDecl);

	VarDefList(node->children[1]);
}

static koopa_raw_value_t VarDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDef);

	char *ident = node->children[0]->data.value.s;

	struct symbol_t *symbol;
	struct view_t view = symbols_lookup(g_symbols, ident);
	for (symbol = view.begin; symbol; symbol = view.next(&view))
		if (symbols_here(g_symbols, symbol))
			break;
	assert(symbol);

	koopa_raw_value_t ret = symbol->variable.raw =
		koopa_raw_alloc(name_local(ident), m_curr_function_ret);

	slice_append(&m_curr_basic_block->insts, ret);

	/* initialized */
	if (node->size == 2)
		slice_append(&m_curr_basic_block->insts,
			     koopa_raw_store(InitVal(node->children[1]), ret));

	return ret;
}

static koopa_raw_value_t InitVal(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_InitVal);

	return Exp(node->children[0]);
}

static void BlockItem(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItem);

	if (node->children[0]->data.kind == AST_Decl)
		Decl(node->children[0]);
	if (node->children[0]->data.kind == AST_Stmt)
		Stmt(node->children[0]);
}

static koopa_raw_value_t LVal(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_LVal);

	char *ident = node->children[0]->data.value.s;
	struct symbol_t *symbol = symbols_get(g_symbols, ident);
	assert(symbol);

	koopa_raw_value_t ret;
	switch (symbol->tag)
	{
	case CONSTANT:
		if (!(ret = symbol->constant.raw))
			ret = symbol->constant.raw =
				koopa_raw_integer(symbol->constant.value);
		break;
	case VARIABLE:
		assert(ret = symbol->variable.raw);
		break;
	default:
		unreachable();
	}

	return ret;
}

static void VarDefList(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDefList);

	for (int i = node->size - 1; i >= 0; --i)
		VarDef(node->children[i]);
}

static void BlockItemList(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItemList);

	for (int i = node->size - 1; i >= 0; --i)
		BlockItem(node->children[i]);
}

/* public defn.s */
koopa_raw_program_t ir(const struct node_t *program)
{
	koopa_raw_program_t ret = CompUnit(program);
	symbols_delete(g_symbols);

	return ret;
}
