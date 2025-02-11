#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "hashtable.h"
#include "exception.h"

/* exported symbol table */
ht_strsym_t g_symbols;

/* state variables */
bool m_constexpr;

/* thrower */
static void semantic_error(const char *fmt, ...)
{
	fprintf(stderr, "Error type C: ");

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

	longjmp(exception_env, 3);
}

/* constructors */
static inline struct symbol_t symbol_constant(int32_t value)
{
	struct symbol_t symbol = {
		.tag = CONSTANT,
		.constant = {
			.value = value,
			.raw = NULL,
		},
	};
	return symbol;
}

static inline struct symbol_t symbol_variable(void)
{
	struct symbol_t symbol = {
		.tag = VARIABLE,
		.variable = {
			.raw = NULL,
		},
	};
	return symbol;
}

static inline struct symbol_t symbol_function(enum function_type_e type)
{
	struct symbol_t symbol = {
		.tag = CONSTANT,
		.function = {
			.type = type,
			.raw = NULL,
		},
	};
	return symbol;
}

/* accessor decl.s */
static void semantic_CompUnit(struct node_t *node);
static void semantic_FuncDef(struct node_t *node);
static enum function_type_e semantic_FuncType(struct node_t *node);
static void semantic_Block(struct node_t *node);
static void semantic_Stmt(struct node_t *node);
static int32_t semantic_Number(struct node_t *node);

static int32_t semantic_Exp(struct node_t *node);
static int32_t semantic_UnaryExp(struct node_t *node);
static int32_t semantic_PrimaryExp(struct node_t *node);

static void semantic_Decl(struct node_t *node);
static void semantic_ConstDecl(struct node_t *node);
static void semantic_BType(struct node_t *node);
static void semantic_ConstDef(struct node_t *node);
static int32_t semantic_ConstInitVal(struct node_t *node);
static void semantic_VarDecl(struct node_t *node);
static void semantic_VarDef(struct node_t *node);
#if 0
/* will be evaluated later in IR generation phase */
static int32_t semantic_InitVal(struct node_t *node);
#endif
static void semantic_BlockItem(struct node_t *node);
static char *semantic_LVal(struct node_t *node);
static int32_t semantic_ConstExp(struct node_t *node);
static void semantic_ConstDefList(struct node_t *node);
static void semantic_VarDefList(struct node_t *node);
static void semantic_BlockItemList(struct node_t *node);

/* accessor defn.s */
void semantic_CompUnit(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_CompUnit);

	semantic_FuncDef(node->children[0]);
}

void semantic_FuncDef(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncDef);

	char *name = node->children[1]->data.value.s;
	if (ht_find(g_symbols, name))
		semantic_error("Redefinition of function `%s`", name);

	enum function_type_e type = semantic_FuncType(node->children[0]);
	ht_upsert(g_symbols, name, symbol_function(type));

	semantic_Block(node->children[2]);
}

static enum function_type_e semantic_FuncType(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_FuncType);

	char *type = node->children[0]->data.value.s;

	if (strcmp(type, "void") == 0)
		assert(false /* todo */);
	if (strcmp(type, "int") == 0)
		return INT;

	assert(false /* unknown FuncType */);
}

static void semantic_Block(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Block);

	semantic_BlockItemList(node->children[0]);
}

void semantic_Stmt(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Stmt);

	switch (node->children[0]->data.kind)
	{
	case AST_LVal:
		{
		char *ident = semantic_LVal(node->children[0]);
		struct symbol_t *symbol = ht_find(g_symbols, ident);
		if (!symbol)
			semantic_error("Undefined symbol: `%s`", ident);
		if (symbol->tag != VARIABLE)
			semantic_error("Assignee must be a variable: `%s`",
				       ident);
		}
		break;
	case AST_RETURN:
		semantic_Exp(node->children[1]);
		break;
	default:
		assert(false /* todo */);
	}
}

static int32_t semantic_Number(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Number);

	return node->children[0]->data.value.i;
}

static int32_t semantic_Exp(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Exp);

	/* unary expression, propagate */
	if (node->size == 1)
		return semantic_UnaryExp(node->children[0]);

	/* otherwise, binary expression */
	char *op_token = node->children[1]->data.value.s;
	uint32_t lhs = semantic_Exp(node->children[0]);
	uint32_t rhs = semantic_Exp(node->children[2]);

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
		assert(false /* unsupported EQOP */);
	case AST_RELOP:
		if (strcmp(op_token, ">") == 0)
			return lhs > rhs;
		if (strcmp(op_token, "<") == 0)
			return lhs < rhs;
		if (strcmp(op_token, ">=") == 0)
			return lhs >= rhs;
		if (strcmp(op_token, "<=") == 0)
			return lhs <= rhs;
		assert(false /* unsupported RELOP */);
	case AST_ADDOP:
		if (strcmp(op_token, "+") == 0)
			return lhs + rhs;
		if (strcmp(op_token, "-") == 0)
			return lhs - rhs;
		assert(false /* unsupported ADDOP */);
	case AST_MULOP:
		if (strcmp(op_token, "*") == 0)
			return lhs * rhs;
		if (strcmp(op_token, "/") == 0)
			return lhs / rhs;
		if (strcmp(op_token, "%") == 0)
			return lhs % rhs;
		assert(false /* unsupported MULOP */);
	default:
		assert(false /* todo */);
	}

	assert(false /* unreachable */);
}

static int32_t semantic_UnaryExp(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_UnaryExp);

	/* sole primary expression (fixed point) */
	if (node->size == 1)
		return semantic_PrimaryExp(node->children[0]);

	/* otherwise, continguous unary expression */
	char op_token = node->children[0]->data.value.s[0];
	uint32_t operand = semantic_UnaryExp(node->children[1]);

	switch (op_token)
	{
	case '+':
		return +operand;
	case '-':
		return -operand;
	case '!':
		return !operand;
	default:
		assert(false /* wrong operator */);
	}
}

static int32_t semantic_PrimaryExp(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_PrimaryExp);

	if (node->children[0]->data.kind == AST_Exp)
		return semantic_Exp(node->children[0]);
	if (node->children[0]->data.kind == AST_Number)
		return semantic_Number(node->children[0]);
	if (node->children[0]->data.kind == AST_LVal)
	{
		char *ident = semantic_LVal(node->children[0]);
		struct symbol_t *symbol = ht_find(g_symbols, ident);
		if (!symbol)
			semantic_error("Undefined symbol: `%s`", ident);
		
		switch (symbol->tag)
		{
		case CONSTANT:
			return symbol->constant.value;
		case VARIABLE:
			if (m_constexpr)
				semantic_error("Constants must be calculated at"
					       " compile time, while `%s` is a "
					       "variable", ident);
			return 0;
		case FUNCTION:
			semantic_error("Function as variable is not supported",
				       ident);
		}
	}

	assert(false /* unreachable */);
}

static void semantic_Decl(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_Decl);

	if (node->children[0]->data.kind == AST_ConstDecl)
		return semantic_ConstDecl(node->children[0]);
	if (node->children[0]->data.kind == AST_VarDecl)
		return semantic_VarDecl(node->children[0]);
}

static void semantic_ConstDecl(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDecl);

	m_constexpr = true;

	semantic_BType(node->children[0]);
	semantic_ConstDefList(node->children[1]);

	m_constexpr = false;
}

static void semantic_BType(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BType);

	if (strcmp(node->children[0]->data.value.s, "int") != 0)
		semantic_error("Type of variable must be `int`");
}

static void semantic_ConstDef(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDef);

	char *ident = node->children[0]->data.value.s;
	if (ht_find(g_symbols, ident))
		semantic_error("Redefinition of constant: `%s`", ident);

	int32_t value = semantic_ConstInitVal(node->children[1]);
	ht_upsert(g_symbols, ident, symbol_constant(value));
}

static int32_t semantic_ConstInitVal(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstInitVal);

	return semantic_ConstExp(node->children[0]);
}

static void semantic_VarDecl(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDecl);

	semantic_BType(node->children[0]);
	semantic_VarDefList(node->children[1]);
}

static void semantic_VarDef(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDef);

	char *ident = node->children[0]->data.value.s;
	if (ht_find(g_symbols, ident))
		semantic_error("Redefinition of variable: `%s`", ident);

	/* always treat as uninitialized, since `InitVal` can only be evaluated
	 * in IR phase because `InitVal` is an `Exp` */
	ht_upsert(g_symbols, ident, symbol_variable());
}

static void semantic_BlockItem(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItem);

	if (node->children[0]->data.kind == AST_Decl)
		return semantic_Decl(node->children[0]);
	if (node->children[0]->data.kind == AST_Stmt)
		return semantic_Stmt(node->children[0]);
}

static char *semantic_LVal(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_LVal);

	return node->children[0]->data.value.s;
}

static int32_t semantic_ConstExp(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstExp);

	return semantic_Exp(node->children[0]);
}

static void semantic_ConstDefList(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_ConstDefList);
	
	for (int i = node->size - 1; i >= 0; --i)
		semantic_ConstDef(node->children[i]);
}

static void semantic_VarDefList(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_VarDefList);

	for (int i = node->size - 1; i >= 0; --i)
		semantic_VarDef(node->children[i]);
}

static void semantic_BlockItemList(struct node_t *node)
{
	assert(node);
	assert(node->data.kind == AST_BlockItemList);

	for (int i = node->size - 1; i >= 0; --i)
		semantic_BlockItem(node->children[i]);
}

void semantic(struct node_t *comp_unit)
{
	g_symbols = ht_strsym_new();

	semantic_CompUnit(comp_unit);
}
