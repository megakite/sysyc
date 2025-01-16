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
struct node_t *comp_unit = NULL;

/* helper function */
static void vfree(int count, ...)
{
	va_list args;
	va_start(args, count);
	for (int i = 0; i < count; ++i) {
		free(va_arg(args, void *));
	}
	va_end(args);
}
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

CompUnit:
	FuncDef {
		// FIXME: yylineno reporting wrong line number
		$$ = ast_nterm(AST_CompUnit, yylineno, 1, $1);
		comp_unit = $$;
	}
	;

FuncDef:
	FuncType IDENT LP RP Block {
		$$ = ast_nterm(AST_FuncDef, yylineno, 3,
			       $1, ast_term(AST_IDENT, $2), $5);
		vfree(3, $2, $3, $4);
	}
	;

FuncType:
	TYPE {
		$$ = ast_nterm(AST_FuncType, yylineno, 1,
			       ast_term(AST_TYPE, $1));
		vfree(1, $1);
	}
	;

Block:
	LC Stmt RC {
		$$ = ast_nterm(AST_Block, yylineno, 1, $2);
		vfree(2, $1, $3);
	}
	;

Stmt:
	RETURN Number SEMI {
		$$ = ast_nterm(AST_Stmt, yylineno, 2,
			       ast_term(AST_RETURN, $1), $2);
		vfree(2, $1, $3);
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
	error = true;
	return fprintf(stderr, "Error type B at Line %d: Unexpected \"%s\"",
		       yylineno, yytext);
}
