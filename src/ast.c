#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ast.h"
#include "macros.h"

extern int yylineno;

static const char *AST_KIND_S[] = {
#if 0
	"YYEMPTY",
#endif
	"YYEOF",
	"YYerror",
	"YYUNDEF",
	"INT_CONST",
	"IDENT",
	"SEMI",
	"TYPE",
	"LP",
	"RP",
	"LC",
	"RC",
	"RETURN",
	"RELOP",
	"EQOP",
	"SHOP",
	"ADDOP",
	"UNARYOP",
	"MULOP",
	"LAND",
	"LOR",
	"CONST",
	"ASSIGN",
	"COMMA",
	"IF",
	"ELSE",
	"WHILE",
	"BREAK",
	"CONTINUE",
	"LB",
	"RB",
	"LOWER_THAN_ELSE",
	"YYACCEPT",
	"CompUnit",
	"FuncDefList",
	"FuncDef",
	"FuncFParamsList",
	"FuncFParam",
	"FuncType",
	"Block",
	"BlockItemList",
	"BlockItem",
	"Stmt",
	"Number",
	"Exp",
	"PrimaryExp",
	"UnaryExp",
	"FuncRParamsList",
	"FuncRParam",
	"Decl",
	"ConstDecl",
	"ConstDefList",
	"BType",
	"ConstDef",
	"ConstInitVal",
	"VarDecl",
	"VarDefList",
	"VarDef",
	"InitVal",
	"LVal",
	"ConstExp",
};

struct node_t *ast_nterm(enum ast_kind_e kind, int count, ...)
{
	assert(kind > AST_YYACCEPT);

	struct node_data_t data = {
		.kind = kind,
		.terminal = false,
		.lineno = yylineno,
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
	assert(kind < AST_YYACCEPT);

	struct node_data_t data = {
		.terminal = true,
		.kind = kind,
		.value.i = value,
	};

	return node_new(data, 0);
}

struct node_t *_ast_term_string(enum ast_kind_e kind, const char *value)
{
	assert(kind < AST_YYACCEPT);

	struct node_data_t data = {
		.terminal = true,
		.kind = kind,
	};
	strncpy(data.value.s, value, IDENT_MAX);
	data.value.s[IDENT_MAX - 1] = '\0';

	return node_new(data, 0);
}

static void lambda_print(void *ptr, int depth)
{
	struct node_t *node = ptr;
	struct node_data_t *data = &node->data;

	for (int i = 0; i < depth * 2; ++i)
		putchar(' ');

	switch (data->kind)
	{
	case AST_INT_CONST:
		printf("%s : %d\n", AST_KIND_S[data->kind], data->value.i);
		break;
	case AST_IDENT:
	case AST_TYPE:
	case AST_RELOP:
	case AST_EQOP:
	case AST_ADDOP:
	case AST_MULOP:
	case AST_UNARYOP:
		printf("%s : %s\n", AST_KIND_S[data->kind], data->value.s);
		break;
	default:
		if (data->terminal)
			printf("%s\n", AST_KIND_S[data->kind]);
		else
			printf("%s (%d)\n", AST_KIND_S[data->kind],
			       data->lineno);
	}
}

void ast_print(struct node_t *node)
{
	node_traverse_depth(node, lambda_print, 0);
}
