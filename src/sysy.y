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
	for (int i = 0; i < count; ++i) {
		free(va_arg(args, void *));
	}
	va_end(args);
}
%}

/* TODO: maybe add float support? */
%union {
	int i;
	int f;
	char *s;
	struct node_t *n;
}

/* tokens */
%token <i> INT_CONST
%token <s> IDENT SEMI COMMA ASSIGNOP RELOP EQOP ADDOP UNARYOP MULOP LAND LOR DOT
	   TYPE LP RP LB RB LC RC STRUCT RETURN IF ELSE WHILE

/* nonterminals */
%type <n> CompUnit FuncDef FuncType Block Stmt Number
	  Exp PrimaryExp UnaryExp

/* association and precedence */
// %right ASSIGNOP
%left LOR
%left LAND
%left EQOP
%left RELOP
%left ADDOP
%left MULOP
%right UNARYOP
// %left LP RP LB RB DOT

/* avoid dangling else */
// %nonassoc LOWER_THAN_ELSE
// %nonassoc ELSE

%%

CompUnit:
	FuncDef {
		/* FIXME: yylineno reporting wrong line number */
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
		free($1);
	}
	;

Block:
	LC Stmt RC {
		$$ = ast_nterm(AST_Block, yylineno, 1, $2);
		vfree(2, $1, $3);
	}
	;

Stmt:
	RETURN Exp SEMI {
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

Exp:
	Exp LOR Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_LOR, $2), $3);
		free($2);
	}
	| Exp LAND Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_LAND, $2), $3);
		free($2);
	}
	| Exp EQOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_EQOP, $2), $3);
		free($2);
	}
	| Exp RELOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_RELOP, $2), $3);
		free($2);
	}
	| Exp ADDOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_ADDOP, $2), $3);
		free($2);
	}
	| Exp MULOP Exp {
		$$ = ast_nterm(AST_Exp, yylineno, 3,
			       $1, ast_term(AST_MULOP, $2), $3);
		free($2);
	}
	| UnaryExp {
		$$ = ast_nterm(AST_Exp, yylineno, 1, $1);
	}
	;

PrimaryExp:
	LP Exp RP {
		$$ = ast_nterm(AST_PrimaryExp, yylineno, 1, $2);
		vfree(2, $1, $3);
	}
	| Number {
		$$ = ast_nterm(AST_PrimaryExp, yylineno, 1, $1);
	}
	;

UnaryExp:
	PrimaryExp {
		$$ = ast_nterm(AST_UnaryExp, yylineno, 1, $1);
	}
	| UNARYOP UnaryExp {
		$$ = ast_nterm(AST_UnaryExp, yylineno, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	| ADDOP UnaryExp {
		// HACK: basically every ADDOP is also a UNARYOP
		$$ = ast_nterm(AST_UnaryExp, yylineno, 2,
			       ast_term(AST_UNARYOP, $1), $2);
		free($1);
	}
	;

%%

int yyerror(char *msg)
{
	error = true;
	return fprintf(stderr, "Error type B at Line %d: Unexpected \"%s\"",
		       yylineno, yytext);
}
