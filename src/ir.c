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
static koopa_raw_function_t m_curr_function;
static size_t m_mangle_idx;

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
static char *mangle(char *ident)
{
	if (!ident)
		return NULL;

	char name[IDENT_MAX];
	snprintf(name, IDENT_MAX, "%s_%hd_%hd_%zu", ident,
		 symbols_level(g_symbols), symbols_scope(g_symbols),
		 m_mangle_idx++);
	name[IDENT_MAX - 1] = '\0';

	return bump_strdup(g_bump, name);
}

static void try_append(koopa_raw_slice_t *slice, koopa_raw_value_t inst)
{
	assert(slice->kind == KOOPA_RSIK_VALUE);

	/* truncate all instructions after branching */
	koopa_raw_value_t last = slice_back(slice);
	if (last
	    && (last->kind.tag == KOOPA_RVT_RETURN
		|| last->kind.tag == KOOPA_RVT_BRANCH
		|| last->kind.tag == KOOPA_RVT_JUMP))
		return;

	slice_append(slice, inst);
}

/* accessor defn.s */
static koopa_raw_type_t FuncType(const struct node_t *node)
{
	assert(node && node->data.kind == AST_FuncType);

	char *type = node->children[0]->data.value.s;

	koopa_raw_type_t ret;
	if (strcmp(type, "int") == 0)
		ret = koopa_raw_type_function(koopa_raw_type_int32());
	else if (strcmp(type, "void") == 0)
		ret = koopa_raw_type_function(koopa_raw_type_unit());
	else
		panic("unsupported FuncType");

	return ret;
}

static void Block(const struct node_t *node)
{
	assert(node && node->data.kind == AST_Block);

	symbols_enter(g_symbols);
	BlockItemList(node->children[0]);
	symbols_dedent(g_symbols);
}

static koopa_raw_value_t PrimaryExp(const struct node_t *node)
{
	assert(node && node->data.kind == AST_PrimaryExp);

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
		try_append(&m_curr_basic_block->insts, ret);
		return ret;
		}
	default:
		todo();
	}
}

static koopa_raw_value_t UnaryExp(const struct node_t *node)
{
	assert(node && node->data.kind == AST_UnaryExp);

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

	try_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t Exp(const struct node_t *node)
{
	assert(node && node->data.kind == AST_Exp);

	/* unary expression, propagate */
	if (node->size == 1)
		return UnaryExp(node->children[0]);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.value.s;
	koopa_raw_value_t lhs = Exp(node->children[0]);
	koopa_raw_value_t rhs = Exp(node->children[2]);

	koopa_raw_value_t ret;
	switch (node->children[1]->data.kind)
	{
	case AST_LOR:
		/* short-circuiting */
		{
		koopa_raw_basic_block_t false_bb =
			koopa_raw_basic_block(mangle("lor_rhs"));
		koopa_raw_basic_block_t end_bb =
			koopa_raw_basic_block(mangle("lor_end"));

		slice_append(&m_curr_function->bbs, false_bb);
		slice_append(&m_curr_function->bbs, end_bb);

		/* non-symbol variable, initialize to 1 (true) */
		koopa_raw_value_t result =
			koopa_raw_alloc(koopa_raw_name_local(mangle("lor")),
					koopa_raw_type_int32());
		koopa_raw_value_t init = koopa_raw_store(koopa_raw_integer(1),
							 result);
		try_append(&m_curr_basic_block->insts, result);
		try_append(&m_curr_basic_block->insts, init);

		/* if (!lhs) */
		koopa_raw_value_t lfalse =
			koopa_raw_binary(KOOPA_RBO_EQ, lhs,
					 koopa_raw_integer(0));
		koopa_raw_value_t branch = koopa_raw_branch(lfalse, false_bb,
							    end_bb);
		try_append(&m_curr_basic_block->insts, lfalse);
		try_append(&m_curr_basic_block->insts, branch);

		/* result = rhs; */
		koopa_raw_value_t rtrue =
			koopa_raw_binary(KOOPA_RBO_NOT_EQ, rhs,
					 koopa_raw_integer(0));
		koopa_raw_value_t store = koopa_raw_store(rtrue, result);
		koopa_raw_value_t jump = koopa_raw_jump(end_bb);

		try_append(&false_bb->insts, rtrue);
		try_append(&false_bb->insts, store);
		try_append(&false_bb->insts, jump);

		m_curr_basic_block = end_bb;

		ret = koopa_raw_load(result);
		}
		break;
	case AST_LAND:
		/* short-circuiting */
		{
		/* non-symbol variable, initialize to 0 (false) */
		koopa_raw_value_t result =
			koopa_raw_alloc(koopa_raw_name_local(mangle("land")),
					koopa_raw_type_int32());
		koopa_raw_value_t init = koopa_raw_store(koopa_raw_integer(0),
							 result);
		try_append(&m_curr_basic_block->insts, result);
		try_append(&m_curr_basic_block->insts, init);

		koopa_raw_basic_block_t true_bb =
			koopa_raw_basic_block(mangle("land_rhs"));
		koopa_raw_basic_block_t end_bb =
			koopa_raw_basic_block(mangle("land_end"));
		slice_append(&m_curr_function->bbs, true_bb);
		slice_append(&m_curr_function->bbs, end_bb);

		/* if (lhs) */
		koopa_raw_value_t ltrue =
			koopa_raw_binary(KOOPA_RBO_NOT_EQ, lhs,
					 koopa_raw_integer(0));
		koopa_raw_value_t branch = koopa_raw_branch(ltrue, true_bb,
							    end_bb);
		try_append(&m_curr_basic_block->insts, ltrue);
		try_append(&m_curr_basic_block->insts, branch);

		/* result = rhs; */
		koopa_raw_value_t rtrue =
			koopa_raw_binary(KOOPA_RBO_NOT_EQ, rhs,
					 koopa_raw_integer(0));
		koopa_raw_value_t store = koopa_raw_store(rtrue, result);
		koopa_raw_value_t jump = koopa_raw_jump(end_bb);

		try_append(&true_bb->insts, rtrue);
		try_append(&true_bb->insts, store);
		try_append(&true_bb->insts, jump);

		m_curr_basic_block = end_bb;

		ret = koopa_raw_load(result);
		}
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
	case AST_SHOP:
		if (strcmp(op_token, ">>") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_SAR, lhs, rhs);
		else if (strcmp(op_token, "<<") == 0)
			ret = koopa_raw_binary(KOOPA_RBO_SHL, lhs, rhs);
		else
			panic("unsupported SHOP");
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

	try_append(&m_curr_basic_block->insts, ret);

	return ret;
}

static koopa_raw_value_t Number(const struct node_t *node)
{
	assert(node && node->data.kind == AST_Number);

	return koopa_raw_integer(node->children[0]->data.value.i);
}

static void Stmt(const struct node_t *node)
{
	assert(node && node->data.kind == AST_Stmt);

	koopa_raw_value_t inst;
	switch (node->children[0]->data.kind)
	{
	case AST_SEMI:
		/* empty Stmt, do nothing */
		break;
	case AST_LVal:
		inst = koopa_raw_store(Exp(node->children[1]),
				       LVal(node->children[0]));

		try_append(&m_curr_basic_block->insts, inst);
		break;
	case AST_Exp:
		Exp(node->children[0]);
		break;
	case AST_Block:
		Block(node->children[0]);
		break;
	case AST_RETURN:
		/* if has `Exp` */
		if (node->size == 2)
			inst = koopa_raw_return(Exp(node->children[1]));
		else
			inst = koopa_raw_return(NULL);

		try_append(&m_curr_basic_block->insts, inst);
		break;
	case AST_IF:
		{
		koopa_raw_value_t cond = Exp(node->children[1]);

		koopa_raw_basic_block_t true_bb =
			koopa_raw_basic_block(mangle("then"));
		koopa_raw_basic_block_t false_bb =
			koopa_raw_basic_block(mangle("else"));
		koopa_raw_basic_block_t end_bb =
			koopa_raw_basic_block(mangle("end"));

		inst = koopa_raw_branch(cond, true_bb, false_bb);
		try_append(&m_curr_basic_block->insts, inst);

		slice_append(&m_curr_function->bbs, true_bb);
		m_curr_basic_block = true_bb;
		// `then` branch always evaluated
		Stmt(node->children[2]);
		try_append(&m_curr_basic_block->insts, koopa_raw_jump(end_bb));
		try_append(&true_bb->insts, koopa_raw_jump(end_bb));

		slice_append(&m_curr_function->bbs, false_bb);
		m_curr_basic_block = false_bb;
		/* if has `else` branch */
		if (node->size == 4)
		{
			Stmt(node->children[3]);
			try_append(&m_curr_basic_block->insts,
				   koopa_raw_jump(end_bb));
		}
		try_append(&false_bb->insts, koopa_raw_jump(end_bb));

		slice_append(&m_curr_function->bbs, end_bb);
		m_curr_basic_block = end_bb;
		}
		break;
	default:
		todo();
	}
}

static koopa_raw_function_t FuncDef(const struct node_t *node)
{
	assert(node && node->data.kind == AST_FuncDef);

	char *name = node->children[1]->data.value.s;

	/* initial basic block */
	koopa_raw_basic_block_t bb = koopa_raw_basic_block("entry");
	m_curr_basic_block = bb;

	koopa_raw_type_t ty = FuncType(node->children[0]);
	koopa_raw_function_t ret = koopa_raw_function(ty, name);
	m_curr_function = ret;
	slice_append(&ret->bbs, bb);

	Block(node->children[2]);

	return ret;
}

static koopa_raw_program_t CompUnit(const struct node_t *node)
{
	assert(node && node->data.kind == AST_CompUnit);

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
	assert(node && node->data.kind == AST_Decl);

	symbols_enter(g_symbols);
	if (node->children[0]->data.kind == AST_VarDecl)
		VarDecl(node->children[0]);
}

static void VarDecl(const struct node_t *node)
{
	assert(node && node->data.kind == AST_VarDecl);

	VarDefList(node->children[1]);
}

static koopa_raw_value_t VarDef(const struct node_t *node)
{
	assert(node && node->data.kind == AST_VarDef);

	char *ident = node->children[0]->data.value.s;

	struct symbol_t *symbol;
	struct view_t view = symbols_lookup(g_symbols, ident);
	for (symbol = view.begin; symbol; symbol = view.next(&view))
		if (symbols_here(g_symbols, symbol))
			break;
	assert(symbol);

	koopa_raw_value_t ret = symbol->variable.raw =
		koopa_raw_alloc(koopa_raw_name_local(mangle(ident)),
				koopa_raw_type_int32());

	try_append(&m_curr_basic_block->insts, ret);

	/* initialized */
	if (node->size == 2)
	{
		/* mind the evaluation order */
		koopa_raw_value_t store =
			koopa_raw_store(InitVal(node->children[1]), ret);
		try_append(&m_curr_basic_block->insts, store);
	}

	return ret;
}

static koopa_raw_value_t InitVal(const struct node_t *node)
{
	assert(node && node->data.kind == AST_InitVal);

	return Exp(node->children[0]);
}

static void BlockItem(const struct node_t *node)
{
	assert(node && node->data.kind == AST_BlockItem);

	if (node->children[0]->data.kind == AST_Decl)
		Decl(node->children[0]);
	if (node->children[0]->data.kind == AST_Stmt)
		Stmt(node->children[0]);
}

static koopa_raw_value_t LVal(const struct node_t *node)
{
	assert(node && node->data.kind == AST_LVal);

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
	assert(node && node->data.kind == AST_VarDefList);

	for (int i = node->size - 1; i >= 0; --i)
		VarDef(node->children[i]);
}

static void BlockItemList(const struct node_t *node)
{
	assert(node && node->data.kind == AST_BlockItemList);

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
