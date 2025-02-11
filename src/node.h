#ifndef _NODE_H_
#define _NODE_H_

#include <stdlib.h>
#include <stdbool.h>

#define IDENT_MAX 32

enum ast_kind_e {
	AST_UNKNOWN = 0,
	AST_INT_CONST,
	AST_IDENT,
	AST_SEMI,
	AST_COMMA,
	AST_ASSIGN,
	AST_RELOP,
	AST_EQOP,
	AST_ADDOP,
	AST_UNARYOP,
	AST_MULOP,
	AST_LAND,
	AST_LOR,
	AST_SHIFTOP,
	AST_TYPE,
	AST_LP,
	AST_RP,
	AST_LB,
	AST_RB,
	AST_LC,
	AST_RC,
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
	AST_Decl,
	AST_ConstDecl,
	AST_BType,
	AST_ConstDef,
	AST_ConstInitVal,
	AST_VarDecl,
	AST_VarDef,
	AST_InitVal,
	AST_BlockItem,
	AST_LVal,
	AST_ConstExp,
	AST_ConstDefList,
	AST_VarDefList,
	AST_BlockItemList,
};

static const char *AST_KIND_S[] = {
	"UNKNOWN",
	"INT_CONST",
	"IDENT",
	"SEMI",
	"COMMA",
	"ASSIGN",
	"RELOP",
	"EQOP",
	"ADDOP",
	"UNARYOP",
	"MULOP",
	"LAND",
	"LOR",
	"SHIFTOP",
	"TYPE",
	"LP",
	"RP",
	"LB",
	"RB",
	"LC",
	"RC",
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
	"Decl",
	"ConstDecl",
	"BType",
	"ConstDef",
	"ConstInitVal",
	"VarDecl",
	"VarDef",
	"InitVal",
	"BlockItem",
	"LVal",
	"ConstExp",
	"ConstDefList",
	"VarDefList",
	"BlockItemList",
};

union ast_value_u {
	int i;
	char s[IDENT_MAX];
};

struct node_data_t {
	bool terminal;
	int lineno;
	enum ast_kind_e kind;
	union ast_value_u value;
};

struct node_t {
	struct node_data_t data;

	int size;
	int capacity;
	struct node_t *children[];
};

struct node_t *node_new(struct node_data_t data, int capacity);

/**
 * Add a child to given root.
 * @return The possibly reallocated root, since we are using flexible members.
 */
struct node_t *node_add_child(struct node_t *root, struct node_t *node);
void node_traverse_post(struct node_t *node, void (*fn)(void *ptr));
void node_traverse_pre_depth(struct node_t *node,
			     void (*fn)(void *ptr, int depth), int depth);
void node_delete(struct node_t *node);

#endif//_NODE_H_
