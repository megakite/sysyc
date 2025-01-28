%option noyywrap
%option nounput
%option noinput
%option yylineno

%{
#include <stdbool.h>

#include "sysy.tab.h"

/* state variables */
bool error = false;
int yycolumn = 1;
#define YY_USER_ACTION						 \
	do {							 \
		yylloc.first_line = yylloc.last_line = yylineno; \
		yylloc.first_column = yycolumn;			 \
		yylloc.last_column = yycolumn + yyleng - 1;	 \
		yycolumn += yyleng;				 \
	} while (0);
%}

WS		[ \t]+
NEWLINE		"\n"|"\r\n"

Comment		"//".*
BlockComment	"/*".*"*/"

/* IDENT */
Identifier	[_A-Za-z][_A-Za-z0-9]*

/* INT_CONST */
Decimal		[1-9][0-9]*
Octal		"0"[0-7]*
Hexadecimal	("0x"|"0X")[0-9A-Fa-f]+

/* RELOP */
Greater		">"
Less		"<"
GreaterEq	">="
LessEq		"<="

/* EQOP */
Eq		"=="
NotEq		"!="

/* ADDOP */
Plus		"+"
Minus		"-"

/* UNARYOP */
Not		"!"

/* MULOP */
Multiply	"*"
Divide		"/"
Modulo		"%"

/* logical operators */
LAnd		"&&"
LOr		"||"

/* bitwise operators */
And		"&"
Or		"|"

/* shift operators */
LLShift		"<<"
LRShift		">>"
ARShift		">>>"

FIXED		[0-9]+"."[0-9]+
SEMI		";"
COMMA		","
ASSIGNOP	"="
DOT		"."
TYPE		"int"|"fixed"
LP		"("
RP		")"
LB		"["
RB		"]"
LC		"{"
RC		"}"
STRUCT		"struct"
RETURN		"return"
IF		"if"
ELSE		"else"
WHILE		"while"

%%

{Comment}
{BlockComment}
{WS}
{NEWLINE}	yycolumn = 1;

{Greater}	{ yylval.s = strdup(yytext); return RELOP; }
{Less}		{ yylval.s = strdup(yytext); return RELOP; }
{GreaterEq}	{ yylval.s = strdup(yytext); return RELOP; }
{LessEq}	{ yylval.s = strdup(yytext); return RELOP; }

{Eq}		{ yylval.s = strdup(yytext); return EQOP; }
{NotEq}		{ yylval.s = strdup(yytext); return EQOP; }

{Plus}		{ yylval.s = strdup(yytext); return ADDOP; }
{Minus}		{ yylval.s = strdup(yytext); return ADDOP; }

{Not}		{ yylval.s = strdup(yytext); return UNARYOP; }

{Multiply}	{ yylval.s = strdup(yytext); return MULOP; }
{Divide}	{ yylval.s = strdup(yytext); return MULOP; }
{Modulo}	{ yylval.s = strdup(yytext); return MULOP; }

{LOr}		{ yylval.s = strdup(yytext); return LOR; }
{LAnd}		{ yylval.s = strdup(yytext); return LAND; }

{SEMI}		{ yylval.s = strdup(yytext); return SEMI; }
{COMMA}		{ yylval.s = strdup(yytext); return COMMA; }
{ASSIGNOP}	{ yylval.s = strdup(yytext); return ASSIGNOP; }
{DOT}		{ yylval.s = strdup(yytext); return DOT; }
{LP}		{ yylval.s = strdup(yytext); return LP; }
{RP}		{ yylval.s = strdup(yytext); return RP; }
{LB}		{ yylval.s = strdup(yytext); return LB; }
{RB}		{ yylval.s = strdup(yytext); return RB; }
{LC}		{ yylval.s = strdup(yytext); return LC; }
{RC}		{ yylval.s = strdup(yytext); return RC; }

{TYPE}		{ yylval.s = strdup(yytext); return TYPE; }
{STRUCT}	{ yylval.s = strdup(yytext); return STRUCT; }
{RETURN}	{ yylval.s = strdup(yytext); return RETURN; }
{IF}		{ yylval.s = strdup(yytext); return IF; }
{ELSE}		{ yylval.s = strdup(yytext); return ELSE; }
{WHILE}		{ yylval.s = strdup(yytext); return WHILE; }

{Decimal}	{ yylval.i = atoi(yytext); return INT_CONST; }
{Octal}		{ yylval.i = strtol(yytext, NULL, 8); return INT_CONST; }
{Hexadecimal}	{ yylval.i = strtol(yytext, NULL, 16); return INT_CONST; }

{Identifier}	{ yylval.s = strdup(yytext); return IDENT; }

.		{ fprintf(stderr, "Error type A at Line %d: Mysterious characte"
		  "r \"%s\"\n", yylineno, yytext); error = true; }

%%
#if 0
{LLShift}	{ yylval.s = strdup(yytext); return SHOP; }
{LRShift}	{ yylval.s = strdup(yytext); return SHOP; }
{ARShift}	{ yylval.s = strdup(yytext); return SHOP; }

{Or}		{ yylval.s = strdup(yytext); return OR; }
{And}		{ yylval.s = strdup(yytext); return AND; }
{Xor}		{ yylval.s = strdup(yytext); return XOR; }
#endif
