%locations

%{
#include <stdio.h>

#include "ast.h"

/* yacc variables */
extern int yylineno;
extern int yylex();
extern int yyerror(char *);
extern char *yytext;

/* state variables */
extern bool error;
struct node_t *comp_unit;

/* helper function */
static void vfree(int count, ...)
{
	va_list args;
	va_start(args, count);
	for (int i = 0; i < count; ++i)
		free(va_arg(args, void *));
	va_end(args);
}
%}

/* TODO maybe add float support? */
%union {
	int i;
	char *s;
	struct node_t *n;
}

/* tokens */
%token <i> INT_CONST
%token <s> IDENT SEMI TYPE LP RP LC RC RETURN
	   RELOP EQOP SHOP ADDOP UNARYOP MULOP LAND LOR
	   CONST ASSIGN COMMA
	   IF ELSE
	   WHILE BREAK CONTINUE
	   LB RB

/* nonterminals */
%type <n> CompUnit FuncDef Type Block Stmt Number
	  Exp PrimaryExp UnaryExp
	  Decl ConstDecl ConstDef ConstInitVal VarDecl VarDef InitVal
	  BlockItem LVal ConstExp ConstDefList VarDefList BlockItemList
	  FuncFParamList FuncFParam FuncRParamList FuncRParam GlobalList Global

/* association and precedence */
%left LOR
%left LAND
%left EQOP
%left RELOP
%left SHOP
%left ADDOP
%left MULOP
%right UNARYOP

/* avoid dangling else */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

CompUnit
	: GlobalList {
		// FIXME yylineno reporting wrong line number
		$$ = ast_nterm(AST_CompUnit, 1, $1);
		comp_unit = $$;
	}
	;

GlobalList
	: Global {
		$$ = ast_nterm(AST_GlobalList, 1, $1);
	}
	| Global GlobalList {
		$$ = node_add_child($2, $1);
	}
	;

Global
	: Decl {
		$$ = ast_nterm(AST_Global, 1, $1);
	}
	| FuncDef {
		$$ = ast_nterm(AST_Global, 1, $1);
	}
	;

FuncDef
	: Type IDENT LP RP Block {
		$$ = ast_nterm(AST_FuncDef, 3,
			       $1, ast_term(AST_IDENT, $2), $5);
		vfree(3, $2, $3, $4);
	}
	| Type IDENT LP FuncFParamList RP Block {
		$$ = ast_nterm(AST_FuncDef, 4,
			       $1, ast_term(AST_IDENT, $2), $4, $6);
		vfree(3, $2, $3, $5);
	}
	;

FuncFParamList
	: FuncFParam {
		$$ = ast_nterm(AST_FuncFParamList, 1, $1);
	}
	| FuncFParam COMMA FuncFParamList {
		$$ = node_add_child($3, $1);
		free($2);
	}
	;

FuncFParam
	: Type IDENT {
		$$ = ast_nterm(AST_FuncFParam, 2, $1, 
			       ast_term(AST_IDENT, $2));
		free($2);
	}
	;

/* context-sensitive Type (rather than BType and FuncType) */
Type
	: TYPE {
		$$ = ast_nterm(AST_Type, 1,
			       ast_term(AST_TYPE, $1));
		free($1);
	}
	;

Block
	: LC BlockItemList RC {
		$$ = ast_nterm(AST_Block, 1, $2);
		vfree(2, $1, $3);
	}
	;

/* define a nullable list in BNF */
BlockItemList
	: /* empty */ {
		$$ = ast_nterm(AST_BlockItemList, 0);
	}
	| BlockItem BlockItemList {
		$$ = node_add_child($2, $1);
	}
	;

BlockItem
	: Decl {
		$$ = ast_nterm(AST_BlockItem, 1, $1);
	}
	| Stmt {
		$$ = ast_nterm(AST_BlockItem, 1, $1);
	}
	;

Stmt
	: LVal ASSIGN Exp SEMI {
		$$ = ast_nterm(AST_Stmt, 2, $1, $3);
		vfree(2, $2, $4);
	}
	| SEMI {
		$$ = ast_nterm(AST_Stmt, 1, ast_term(AST_SEMI, $1));
		free($1);
	}
	| Exp SEMI {
		$$ = ast_nterm(AST_Stmt, 1, $1);
		free($2);
	}
	| Block {
		$$ = ast_nterm(AST_Stmt, 1, $1);
	}
        | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE {
		$$ = ast_nterm(AST_Stmt, 3,
			       ast_term(AST_IF, $1), $3, $5);
		vfree(3, $1, $2, $4);
	}
	| IF LP Exp RP Stmt ELSE Stmt {
		$$ = ast_nterm(AST_Stmt, 4,
			       ast_term(AST_IF, $1), $3, $5, $7);
		vfree(4, $1, $2, $4, $6);
	}
	| WHILE LP Exp RP Stmt {
		$$ = ast_nterm(AST_Stmt, 3,
			       ast_term(AST_WHILE, $1), $3, $5);
		vfree(3, $1, $2, $4);
	}
	| BREAK SEMI {
		$$ = ast_nterm(AST_Stmt, 1, ast_term(AST_BREAK, $1));
		vfree(2, $1, $2);
	}
	| CONTINUE SEMI {
		$$ = ast_nterm(AST_Stmt, 1,
			       ast_term(AST_CONTINUE, $1));
		vfree(2, $1, $2);
	}
	| RETURN SEMI {
		$$ = ast_nterm(AST_Stmt, 1, ast_term(AST_RETURN, $1));
		vfree(2, $1, $2);
	}
	| RETURN Exp SEMI {
		$$ = ast_nterm(AST_Stmt, 2,
			       ast_term(AST_RETURN, $1), $2);
		vfree(2, $1, $3);
	}
	;

Number
	: INT_CONST {
		$$ = ast_nterm(AST_Number, 1,
			       ast_term(AST_INT_CONST, $1));
	}
	;

Exp
	: Exp LOR Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_LOR, $2), $3);
		free($2);
	}
	| Exp LAND Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_LAND, $2), $3);
		free($2);
	}
	| Exp EQOP Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_EQOP, $2), $3);
		free($2);
	}
	| Exp RELOP Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_RELOP, $2), $3);
		free($2);
	}
	| Exp SHOP Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_SHOP, $2), $3);
		free($2);
	}
	| Exp ADDOP Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_ADDOP, $2), $3);
		free($2);
	}
	| Exp MULOP Exp {
		$$ = ast_nterm(AST_Exp, 3, $1,
			       ast_term(AST_MULOP, $2), $3);
		free($2);
	}
	| UnaryExp {
		$$ = ast_nterm(AST_Exp, 1, $1);
	}
	;

PrimaryExp
	: LP Exp RP {
		$$ = ast_nterm(AST_PrimaryExp, 1, $2);
		vfree(2, $1, $3);
	}
	| LVal {
		$$ = ast_nterm(AST_PrimaryExp, 1, $1);
	}
	| Number {
		$$ = ast_nterm(AST_PrimaryExp, 1, $1);
	}
	;

UnaryExp
	: PrimaryExp {
		$$ = ast_nterm(AST_UnaryExp, 1, $1);
	}
	| UNARYOP UnaryExp {
		$$ = ast_nterm(AST_UnaryExp, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	| ADDOP UnaryExp {
		// basically every ADDOP is also a UNARYOP
		$$ = ast_nterm(AST_UnaryExp, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	| IDENT LP FuncRParamList RP {
		$$ = ast_nterm(AST_UnaryExp, 2,
			       ast_term(AST_IDENT, $1), $3);
		vfree(3, $1, $2, $4);
	}
	| IDENT LP RP {
		$$ = ast_nterm(AST_UnaryExp, 1,
			       ast_term(AST_IDENT, $1));
		vfree(3, $1, $2, $3);
	}
	;

FuncRParamList
	: FuncRParam {
		$$ = ast_nterm(AST_FuncRParamList, 1, $1);
	}
	| FuncRParam COMMA FuncRParamList {
		$$ = node_add_child($3, $1);
		free($2);
	}
	;

FuncRParam
	: Exp {
		$$ = ast_nterm(AST_FuncRParam, 1, $1);
	}
	;

Decl
	: ConstDecl {
		$$ = ast_nterm(AST_Decl, 1, $1);
	}
	| VarDecl {
		$$ = ast_nterm(AST_Decl, 1, $1);
	}
	;

ConstDecl
	: CONST Type ConstDefList SEMI {
		$$ = ast_nterm(AST_ConstDecl, 2, $2, $3);
		vfree(2, $1, $4);
	}
	;

/* define a non-empty list in BNF */
ConstDefList
	: ConstDef {
		$$ = ast_nterm(AST_ConstDefList, 1, $1);
	}
	| ConstDef COMMA ConstDefList {
		$$ = node_add_child($3, $1);
		free($2);
	}
	;

ConstDef
	: IDENT ASSIGN ConstInitVal {
		$$ = ast_nterm(AST_ConstDef, 2,
			       ast_term(AST_IDENT, $1), $3);
		vfree(2, $1, $2);
	}
	;

ConstInitVal
	: ConstExp {
		$$ = ast_nterm(AST_ConstInitVal, 1, $1);
	}
	;

VarDecl
	: Type VarDefList SEMI {
		$$ = ast_nterm(AST_VarDecl, 2, $1, $2);
		free($3);
	}
	;

VarDefList
	: VarDef {
		$$ = ast_nterm(AST_VarDefList, 1, $1);
	}
	| VarDef COMMA VarDefList {
		$$ = node_add_child($3, $1);
		free($2);
	}
	;

VarDef
	: IDENT {
		$$ = ast_nterm(AST_VarDef, 1,
			       ast_term(AST_IDENT, $1));
		free($1);
	}
	| IDENT ASSIGN InitVal {
		$$ = ast_nterm(AST_VarDef, 2,
			       ast_term(AST_IDENT, $1), $3);
		vfree(2, $1, $2);
	}
	;

InitVal
	: Exp {
		$$ = ast_nterm(AST_InitVal, 1, $1);
	}
	;

LVal
	: IDENT {
		$$ = ast_nterm(AST_LVal, 1, ast_term(AST_IDENT, $1));
		free($1);
	}
	;

ConstExp
	: Exp {
		$$ = ast_nterm(AST_ConstExp, 1, $1);
	}
	;

%%

int yyerror(char *msg)
{
	(void) msg;

	error = true;
	return fprintf(stderr, "Syntax error at line %d: unexpected `%s`\n",
		       yylineno, yytext);
}
