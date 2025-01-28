#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdbool.h>

#define IDENT_MAX 32

enum ast_kind_e {
	AST_UNKNOWN = 0,
	AST_INT_CONST,
	AST_FLOAT,
	AST_IDENT,
	AST_SEMI,
	AST_COMMA,
	AST_ASSIGNOP,
	AST_RELOP,
	AST_EQOP,
	AST_ADDOP,
	AST_UNARYOP,
	AST_MULOP,
	AST_LAND,
	AST_LOR,
	AST_DOT,
	AST_TYPE,
	AST_LP,
	AST_RP,
	AST_LB,
	AST_RB,
	AST_LC,
	AST_RC,
	AST_STRUCT,
	AST_RETURN,
	AST_IF,
	AST_ELSE,
	AST_WHILE,
	AST_END_OF_TERM,
	AST_CompUnit,
	AST_FuncDef,
	AST_FuncType,
	AST_Block,
	AST_Stmt,
	AST_Number,
	AST_Exp,
	AST_PrimaryExp,
	AST_UnaryExp,
};

static const char *AST_KIND_S[] = {
	"UNKNOWN",
	"INT_CONST",
	"FLOAT",
	"IDENT",
	"SEMI",
	"COMMA",
	"ASSIGNOP",
	"RELOP",
	"EQOP",
	"ADDOP",
	"UNARYOP",
	"MULOP",
	"LAND",
	"LOR",
	"DOT",
	"TYPE",
	"LP",
	"RP",
	"LB",
	"RB",
	"LC",
	"RC",
	"STRUCT",
	"RETURN",
	"IF",
	"ELSE",
	"WHILE",
	"END_OF_TERM",
	"CompUnit",
	"FuncDef",
	"FuncType",
	"Block",
	"Stmt",
	"Number",
	"Exp",
	"PrimaryExp",
	"UnaryExp",
};

union ast_value_u {
	int i;
	char s[IDENT_MAX];
};

struct ast_data_t {
	bool terminal;
	int lineno;
	enum ast_kind_e kind;
	union ast_value_u value;
};

#endif//_TYPES_H_
