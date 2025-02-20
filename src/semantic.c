#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "hashtable.h"
#include "semantic.h"
#include "symbols.h"
#include "macros.h"

/* state variables */
static bool m_constexpr;
static const struct node_t *m_this_node;

/* thrower */
static void error(const char *fmt, ...)
{
	fprintf(stderr, "Line %d: ", m_this_node->data.lineno);

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

	longjmp(exception_env, 3);
}

/* ctor.s */
static struct symbol_t symbol_constant(int32_t value)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = CONSTANT,
		.constant = {
			.value = value,
			.raw = NULL,
		},
	};
	return symbol;
}

static struct symbol_t symbol_variable(void)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = VARIABLE,
		.variable = {
			.raw = NULL,
		},
	};
	return symbol;
}

static struct symbol_t symbol_function(enum function_type_e type)
{
	struct symbol_t symbol = {
		.meta = {
			.level = symbols_level(g_symbols),
			.scope = symbols_scope(g_symbols),
		},
		.tag = CONSTANT,
		.function = {
			.type = type,
			.raw = NULL,
		},
	};
	return symbol;
}

/* accessor decl.s */
static void CompUnit(const struct node_t *node);
static void FuncDef(const struct node_t *node);
static enum function_type_e FuncType(const struct node_t *node);
static void Block(const struct node_t *node);
static void Stmt(const struct node_t *node);
static int32_t Number(const struct node_t *node);

static int32_t Exp(const struct node_t *node);
static int32_t UnaryExp(const struct node_t *node);
static int32_t PrimaryExp(const struct node_t *node);

static void Decl(const struct node_t *node);
static void ConstDecl(const struct node_t *node);
static void BType(const struct node_t *node);
static void ConstDef(const struct node_t *node);
static int32_t ConstInitVal(const struct node_t *node);
static void VarDecl(const struct node_t *node);
static void VarDef(const struct node_t *node);
#if 0
/* will be evaluated later in IR generation phase */
static int32_t InitVal(const struct node_t *node);
#endif
static void BlockItem(const struct node_t *node);
static char *LVal(const struct node_t *node);
static int32_t ConstExp(const struct node_t *node);
static void ConstDefList(const struct node_t *node);
static void VarDefList(const struct node_t *node);
static void BlockItemList(const struct node_t *node);

/* accessor defn.s */
static void CompUnit(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_CompUnit);
	m_this_node = node;

	symbols_indent(g_symbols);
	FuncDef(node->children[0]);
	symbols_leave(g_symbols);
}

static void FuncDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncDef);
	m_this_node = node;

	char *name = node->children[1]->data.value.s;

	struct view_t view = symbols_lookup(g_symbols, name);
	for (struct symbol_t *it = view.begin; it; it = view.next(&view))
		if (symbols_here(g_symbols, it))
			error("Redefinition of function: `%s`", name);

	enum function_type_e type = FuncType(node->children[0]);
	symbols_add(g_symbols, name, symbol_function(type));

	Block(node->children[2]);
}

static enum function_type_e FuncType(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncType);
	m_this_node = node;

	char *type = node->children[0]->data.value.s;

	if (strcmp(type, "void") == 0)
		todo();
	if (strcmp(type, "int") == 0)
		return INT;

	panic("unsupported FuncType");
}

static void Block(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Block);
	m_this_node = node;

	symbols_indent(g_symbols);
	BlockItemList(node->children[0]);
	symbols_leave(g_symbols);
}

static void Stmt(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Stmt);
	m_this_node = node;

	switch (node->children[0]->data.kind)
	{
	case AST_SEMI:
		/* empty Stmt, do nothing */
		break;
	case AST_Exp:
		Exp(node->children[0]);
		break;
	case AST_LVal:
		{
		char *ident = LVal(node->children[0]);

		struct symbol_t *symbol = symbols_get(g_symbols, ident);
		if (!symbol)
			error("Undefined symbol: `%s`", ident);
		if (symbol->tag != VARIABLE)
			error("Assignee must be a variable: `%s`", ident);
		}
		break;
	case AST_Block:
		Block(node->children[0]);
		break;
	case AST_RETURN:
		/* RETURN [Exp] SEMI */
		if (node->size == 2)
			Exp(node->children[1]);
		break;
	default:
		todo();
	}
}

static int32_t Number(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Number);
	m_this_node = node;

	return node->children[0]->data.value.i;
}

static int32_t Exp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Exp);
	m_this_node = node;

	/* unary expression, propagate */
	if (node->size == 1)
		return UnaryExp(node->children[0]);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.value.s;
	uint32_t lhs = Exp(node->children[0]);
	uint32_t rhs = Exp(node->children[2]);

	/* logical operators are kinda naughty */
	switch (node->children[1]->data.kind)
	{
	case AST_LOR:
		return lhs || rhs;
	case AST_LAND:
		return lhs && rhs;
	case AST_EQOP:
		if (strcmp(op_token, "!=") == 0)
			return lhs != rhs;
		if (strcmp(op_token, "==") == 0)
			return lhs == rhs;
		panic("unsupported EQOP");
	case AST_RELOP:
		if (strcmp(op_token, ">") == 0)
			return lhs > rhs;
		if (strcmp(op_token, "<") == 0)
			return lhs < rhs;
		if (strcmp(op_token, ">=") == 0)
			return lhs >= rhs;
		if (strcmp(op_token, "<=") == 0)
			return lhs <= rhs;
		panic("unsupported RELOP");
	case AST_ADDOP:
		if (strcmp(op_token, "+") == 0)
			return lhs + rhs;
		if (strcmp(op_token, "-") == 0)
			return lhs - rhs;
		panic("unsupported ADDOP");
	case AST_MULOP:
		if (strcmp(op_token, "*") == 0)
			return lhs * rhs;
		if (strcmp(op_token, "/") == 0)
			return lhs / rhs;
		if (strcmp(op_token, "%") == 0)
			return lhs % rhs;
		panic("unsupported MULOP");
	default:
		todo();
	}
}

static int32_t UnaryExp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_UnaryExp);
	m_this_node = node;

	/* sole primary expression (fixed point) */
	if (node->size == 1)
		return PrimaryExp(node->children[0]);

	/* otherwise, continguous unary expression */
	char op_token = node->children[0]->data.value.s[0];
	uint32_t operand = UnaryExp(node->children[1]);

	switch (op_token)
	{
	case '+':
		return +operand;
	case '-':
		return -operand;
	case '!':
		return !operand;
	default:
		panic("unsupported unary operator");
	}
}

static int32_t PrimaryExp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_PrimaryExp);
	m_this_node = node;

	if (node->children[0]->data.kind == AST_Exp)
		return Exp(node->children[0]);
	if (node->children[0]->data.kind == AST_Number)
		return Number(node->children[0]);
	if (node->children[0]->data.kind == AST_LVal)
	{
		char *ident = LVal(node->children[0]);

		struct symbol_t *symbol = symbols_get(g_symbols, ident);
		if (!symbol)
			error("Undefined symbol: `%s`", ident);
		
		switch (symbol->tag)
		{
		case CONSTANT:
			return symbol->constant.value;
		case VARIABLE:
			if (m_constexpr)
				error("Constants must be evaluated at compile t"
				      "ime, while `%s` is a variable", ident);
			return 0;
		case FUNCTION:
			error("Function as variable is not supported", ident);
		}
	}

	unreachable();
}

static void Decl(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Decl);
	m_this_node = node;

	symbols_indent(g_symbols);
	if (node->children[0]->data.kind == AST_ConstDecl)
		ConstDecl(node->children[0]);
	if (node->children[0]->data.kind == AST_VarDecl)
		VarDecl(node->children[0]);
	// no need to `leave()` here, we'll clean it up when we exit this block
}

static void ConstDecl(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDecl);
	m_this_node = node;

	m_constexpr = true;

	BType(node->children[0]);
	ConstDefList(node->children[1]);

	m_constexpr = false;
}

static void BType(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BType);
	m_this_node = node;

	if (strcmp(node->children[0]->data.value.s, "int") != 0)
		error("Type of variable must be `int`");
}

static void ConstDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDef);
	m_this_node = node;

	char *ident = node->children[0]->data.value.s;

	struct view_t view = symbols_lookup(g_symbols, ident);
	for (struct symbol_t *it = view.begin; it; it = view.next(&view))
		if (symbols_here(g_symbols, it))
			error("Redefinition of constant: `%s`", ident);

	int32_t value = ConstInitVal(node->children[1]);
	symbols_add(g_symbols, ident, symbol_constant(value));
}

static int32_t ConstInitVal(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstInitVal);
	m_this_node = node;

	return ConstExp(node->children[0]);
}

static void VarDecl(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDecl);
	m_this_node = node;

	BType(node->children[0]);
	VarDefList(node->children[1]);
}

static void VarDef(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDef);
	m_this_node = node;

	char *ident = node->children[0]->data.value.s;

	struct view_t view = symbols_lookup(g_symbols, ident);
	for (struct symbol_t *it = view.begin; it; it = view.next(&view))
		if (symbols_here(g_symbols, it))
			error("Redefinition of variable: `%s`", ident);

	/* always treat as uninitialized, since `InitVal` can only be evaluated
	 * in IR phase because `InitVal` is an `Exp` */
	symbols_add(g_symbols, ident, symbol_variable());
}

static void BlockItem(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItem);
	m_this_node = node;

	if (node->children[0]->data.kind == AST_Decl)
		Decl(node->children[0]);
	if (node->children[0]->data.kind == AST_Stmt)
		Stmt(node->children[0]);
}

static char *LVal(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_LVal);
	m_this_node = node;

	return node->children[0]->data.value.s;
}

static int32_t ConstExp(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstExp);
	m_this_node = node;

	return Exp(node->children[0]);
}

static void ConstDefList(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDefList);
	m_this_node = node;
	
	for (int i = node->size - 1; i >= 0; --i)
		ConstDef(node->children[i]);
}

static void VarDefList(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDefList);
	m_this_node = node;

	for (int i = node->size - 1; i >= 0; --i)
		VarDef(node->children[i]);
}

static void BlockItemList(const struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItemList);
	m_this_node = node;

	for (int i = node->size - 1; i >= 0; --i)
		BlockItem(node->children[i]);
}

void semantic(const struct node_t *comp_unit)
{
	g_symbols = symbols_new();

	CompUnit(comp_unit);
}
