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
struct node_t *program = NULL;

/* variables for flattening lists, currently unused */
struct node_t *this_ExtDefList = NULL;
struct node_t *this_ExtDecList = NULL;
struct node_t *this_VarDec = NULL;
struct node_t *this_VarList = NULL;
struct node_t *this_DefList = NULL;
struct node_t *this_DecList = NULL;
struct node_t *this_StmtList = NULL;
struct node_t *this_Args = NULL;
%}

%union {
	int i;
	float f;
	char *s;
	struct node_t *n;
}

/* tokens */
%token <i> INT_CONST
%token <f> FLOAT
%token <s> IDENT SEMI COMMA ASSIGNOP RELOP PLUS MINUS STAR DIV AND OR DOT NOT
	   TYPE LP RP LB RB LC RC STRUCT RETURN IF ELSE WHILE

/* nonterminals */
%type <n> CompUnit FuncDef FuncType Block Stmt Number

/* association and precedence */
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
// TODO: treat unary negation rightly
%right NOT // MINUS
%left LP RP LB RB DOT

/* avoid dangling else */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

/* Lv1 */
CompUnit:
	FuncDef {
		// FIXME: yylineno reporting wrong line number
		$$ = ast_nterm(AST_CompUnit, yylineno, 1, $1);
		program = $$;
	}
	;
FuncDef:
	FuncType IDENT LP RP Block {
		$$ = ast_nterm(AST_FuncDef, yylineno, 3,
			       $1, ast_term(AST_IDENT, $2), $5);
		free($2);
		free($3);
		free($4);
	}
	;
FuncType:
	TYPE {
		$$ = ast_nterm(AST_FuncType, yylineno, 1,
			       ast_term(AST_TYPE, $1));
		free($1);
	}
	;
Block:
	LC Stmt RC {
		$$ = ast_nterm(AST_Block, yylineno, 1, $2);
		free($1);
		free($3);
	}
	;
Stmt:
	RETURN Number SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 2,
			       ast_term(AST_RETURN, $1), $2);
		free($1);
		free($3);
	}
	;
Number:
	INT_CONST {
		$$ = ast_nterm(AST_Number, yylineno, 1,
			       ast_term(AST_INT_CONST, $1));
	}
	;

%%

int yyerror(char *msg)
{
	fprintf(stderr, "Error type B at Line %d: Unexpected \"%s\"", yylineno,
		yytext);

	error = true;
}
