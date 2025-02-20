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

/* list flattening */
static struct node_t *this_BlockItemList;
static struct node_t *this_ConstDefList;
static struct node_t *this_VarDefList;

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

/* TODO: maybe add float support? */
%union {
	int i;
	char *s;
	struct node_t *n;
}

/* tokens */
%token <i> INT_CONST
%token <s> IDENT SEMI TYPE LP RP LC RC RETURN
	   RELOP EQOP ADDOP UNARYOP MULOP LAND LOR
	   CONST ASSIGN
	   IF ELSE
	   WHILE
	   COMMA
	   LB RB

/* nonterminals */
%type <n> CompUnit FuncDef FuncType Block Stmt Number
	  Exp PrimaryExp UnaryExp
	  Decl ConstDecl BType ConstDef ConstInitVal VarDecl VarDef InitVal
	  BlockItem LVal ConstExp ConstDefList VarDefList BlockItemList

/* association and precedence */
%left LOR
%left LAND
%left EQOP
%left RELOP
%left ADDOP
%left MULOP
%right UNARYOP

/* avoid dangling else */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

CompUnit
	: FuncDef {
		// FIXME: yylineno reporting wrong line number
		$$ = ast_nterm(AST_CompUnit, yylineno, 1, $1);
		comp_unit = $$;
	}
	;

FuncDef
	: FuncType IDENT LP RP Block {
		$$ = ast_nterm(AST_FuncDef, yylineno, 3,
			       $1, ast_term(AST_IDENT, $2), $5);
		vfree(3, $2, $3, $4);
	}
	;

FuncType
	: TYPE {
		$$ = ast_nterm(AST_FuncType, yylineno, 1,
			       ast_term(AST_TYPE, $1));
		free($1);
	}
	;

Block
	: LC BlockItemList RC {
		$$ = ast_nterm(AST_Block, yylineno, 1, $2);
		vfree(2, $1, $3);
	}
	;

/* define a nullable list in BNF */
BlockItemList
	: /* empty */ {
		$$ = this_BlockItemList = ast_nterm(AST_BlockItemList, yylineno,
						    0);
	}
	| BlockItem BlockItemList {
		$$ = this_BlockItemList = node_add_child(this_BlockItemList,
							 $1);
	}
	;

BlockItem
	: Decl {
		$$ = ast_nterm(AST_BlockItem, yylineno, 1, $1);
	}
	| Stmt {
		$$ = ast_nterm(AST_BlockItem, yylineno, 1, $1);
	}
	;

Stmt
	: SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 1, ast_term(AST_SEMI, $1));
		free($1);
	}
        | LVal ASSIGN Exp SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 2, $1, $3);
		vfree(2, $2, $4);
	}
	| Exp SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 1, $1);
		free($2);
	}
	| Block {
		$$ = ast_nterm(AST_Stmt, yylineno, 1, $1);
	}
	| RETURN SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 1, ast_term(AST_RETURN, $1));
		vfree(2, $1, $2);
	}
	| RETURN Exp SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 2,
			       ast_term(AST_RETURN, $1), $2);
		vfree(2, $1, $3);
	}
	;

Number
	: INT_CONST {
		$$ = ast_nterm(AST_Number, yylineno, 1,
			       ast_term(AST_INT_CONST, $1));
	}
	;

Exp
	: Exp LOR Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_LOR, $2), $3);
		free($2);
	}
	| Exp LAND Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_LAND, $2), $3);
		free($2);
	}
	| Exp EQOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_EQOP, $2), $3);
		free($2);
	}
	| Exp RELOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_RELOP, $2), $3);
		free($2);
	}
	| Exp ADDOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_ADDOP, $2), $3);
		free($2);
	}
	| Exp MULOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3, $1,
			       ast_term(AST_MULOP, $2), $3);
		free($2);
	}
	| UnaryExp {
		$$ = ast_nterm(AST_Exp, yylineno, 1, $1);
	}
	;

PrimaryExp
	: LP Exp RP {
		$$ = ast_nterm(AST_PrimaryExp, yylineno, 1, $2);
		vfree(2, $1, $3);
	}
	| LVal {
		$$ = ast_nterm(AST_PrimaryExp, yylineno, 1, $1);
	}
	| Number {
		$$ = ast_nterm(AST_PrimaryExp, yylineno, 1, $1);
	}
	;

UnaryExp
	: PrimaryExp {
		$$ = ast_nterm(AST_UnaryExp, yylineno, 1, $1);
	}
	| UNARYOP UnaryExp {
		$$ = ast_nterm(AST_UnaryExp, yylineno, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	| ADDOP UnaryExp {
		// basically every ADDOP is also a UNARYOP
		$$ = ast_nterm(AST_UnaryExp, yylineno, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	;

Decl
	: ConstDecl {
		$$ = ast_nterm(AST_Decl, yylineno, 1, $1);
	}
	| VarDecl {
		$$ = ast_nterm(AST_Decl, yylineno, 1, $1);
	}
	;

ConstDecl
	: CONST BType ConstDefList SEMI {
		$$ = ast_nterm(AST_ConstDecl, yylineno, 2, $2, $3);
		vfree(2, $1, $4);
	}
	;

/* define a non-empty list in BNF */
ConstDefList
	: ConstDef {
		$$ = this_ConstDefList = ast_nterm(AST_ConstDefList, yylineno,
						   1, $1);
	}
	| ConstDef COMMA ConstDefList {
		$$ = this_ConstDefList = node_add_child(this_ConstDefList, $1);
		free($2);
	}
	;

BType
	: TYPE {
		$$ = ast_nterm(AST_BType, yylineno, 1, ast_term(AST_TYPE, $1));
		free($1);
	}
	;

ConstDef
	: IDENT ASSIGN ConstInitVal {
		$$ = ast_nterm(AST_ConstDef, yylineno, 2,
			       ast_term(AST_IDENT, $1), $3);
		vfree(2, $1, $2);
	}
	;

ConstInitVal
	: ConstExp {
		$$ = ast_nterm(AST_ConstInitVal, yylineno, 1, $1);
	}
	;

VarDecl
	: BType VarDefList SEMI {
		$$ = ast_nterm(AST_VarDecl, yylineno, 2, $1, $2);
		free($3);
	}
	;

VarDefList
	: VarDef {
		$$ = this_VarDefList = ast_nterm(AST_VarDefList, yylineno, 1,
						 $1);
	}
	| VarDef COMMA VarDefList {
		$$ = this_VarDefList = node_add_child(this_VarDefList, $1);
		free($2);
	}
	;

VarDef
	: IDENT {
		$$ = ast_nterm(AST_VarDef, yylineno, 1,
			       ast_term(AST_IDENT, $1));
		free($1);
	}
	| IDENT ASSIGN InitVal {
		$$ = ast_nterm(AST_VarDef, yylineno, 2,
			       ast_term(AST_IDENT, $1), $3);
		vfree(2, $1, $2);
	}
	;

InitVal
	: Exp {
		$$ = ast_nterm(AST_InitVal, yylineno, 1, $1);
	}
	;

LVal
	: IDENT {
		$$ = ast_nterm(AST_LVal, yylineno, 1, ast_term(AST_IDENT, $1));
		free($1);
	}
	;

ConstExp
	: Exp {
		$$ = ast_nterm(AST_ConstExp, yylineno, 1, $1);
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
