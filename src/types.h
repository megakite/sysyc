/**
 * Type extensions for AST.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdbool.h>

#define ID_MAX 32

enum ast_kind_e {
	AST_UNKNOWN = 0,
	AST_INT_CONST,
	AST_FLOAT,
	AST_IDENT,
	AST_SEMI,
	AST_COMMA,
	AST_ASSIGNOP,
	AST_RELOP,
	AST_PLUS,
	AST_MINUS,
	AST_STAR,
	AST_DIV,
	AST_AND,
	AST_OR,
	AST_DOT,
	AST_NOT,
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
};

static const char *AST_KIND_STRINGS[] = {
	"UNKNOWN",
	"INT_CONST",
	"FLOAT",
	"IDENT",
	"SEMI",
	"COMMA",
	"ASSIGNOP",
	"RELOP",
	"PLUS",
	"MINUS",
	"STAR",
	"DIV",
	"AND",
	"OR",
	"DOT",
	"NOT",
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
};

union ast_value_u {
	int i;
	float f;
	char s[ID_MAX];
};

struct ast_data_t {
	bool terminal;
	int line;
	enum ast_kind_e kind;
	union ast_value_u value;
};

#endif//_TYPES_H_
