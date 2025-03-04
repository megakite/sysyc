/**
 * node.h
 * Definition and implementation of the node system.
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <stdlib.h>
#include <stdbool.h>

// ima be lazy here, obliging what the standard says
#define IDENT_MAX 64

enum ast_kind_e {
#if 0
	AST_YYEMPTY = -2,
#endif
	AST_YYEOF = 0,
	AST_YYerror = 1,
	AST_YYUNDEF = 2,
	AST_INT_CONST = 3,
	AST_IDENT = 4,
	AST_SEMI = 5,
	AST_TYPE = 6,
	AST_LP = 7,
	AST_RP = 8,
	AST_LC = 9,
	AST_RC = 10,
	AST_RETURN = 11,
	AST_RELOP = 12,
	AST_EQOP = 13,
	AST_SHOP = 14,
	AST_ADDOP = 15,
	AST_UNARYOP = 16,
	AST_MULOP = 17,
	AST_LAND = 18,
	AST_LOR = 19,
	AST_CONST = 20,
	AST_ASSIGN = 21,
	AST_COMMA = 22,
	AST_IF = 23,
	AST_ELSE = 24,
	AST_WHILE = 25,
	AST_BREAK = 26,
	AST_CONTINUE = 27,
	AST_LB = 28,
	AST_RB = 29,
	AST_LOWER_THAN_ELSE = 30,
	AST_YYACCEPT = 31,
	AST_CompUnit = 32,
	AST_FuncDefList = 33,
	AST_FuncDef = 34,
	AST_FuncFParamsList = 35,
	AST_FuncFParam = 36,
	AST_FuncType = 37,
	AST_Block = 38,
	AST_BlockItemList = 39,
	AST_BlockItem = 40,
	AST_Stmt = 41,
	AST_Number = 42,
	AST_Exp = 43,
	AST_PrimaryExp = 44,
	AST_UnaryExp = 45,
	AST_FuncRParamsList = 46,
	AST_FuncRParam = 47,
	AST_Decl = 48,
	AST_ConstDecl = 49,
	AST_ConstDefList = 50,
	AST_BType = 51,
	AST_ConstDef = 52,
	AST_ConstInitVal = 53,
	AST_VarDecl = 54,
	AST_VarDefList = 55,
	AST_VarDef = 56,
	AST_InitVal = 57,
	AST_LVal = 58,
	AST_ConstExp = 59,
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

/* Add a child to given root.
 * @return Possibly reallocated root.
 */
struct node_t *node_add_child(struct node_t *root, struct node_t *node);
void node_traverse_post(struct node_t *node, void (*fn)(void *ptr));
void node_traverse_depth(struct node_t *node,
			     void (*fn)(void *ptr, int depth), int depth);
void node_delete(struct node_t *node);

#endif//_NODE_H_
