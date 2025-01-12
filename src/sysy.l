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
#define YY_USER_ACTION \
	yylloc.first_line = yylloc.last_line = yylineno; \
	yylloc.first_column = yycolumn; \
	yylloc.last_column = yycolumn + yyleng - 1; \
	yycolumn += yyleng;
%}

WS		[ \t]+
NEWLINE		"\n"|"\r\n"

Comment		"//".*
MultiComment	"/*".*"*/"

/* IDENT */
Identifier	[_A-Za-z][_A-Za-z0-9]*

/* INT_CONST */
Decimal		[1-9][0-9]*
Octal		"0"[0-7]*
Hexadecimal	("0x"|"0X")[0-9A-Fa-f]+

FLOAT		[0-9]+"."|"."[0-9]+|[0-9]+"."[0-9]+
SEMI		";"
COMMA		","
ASSIGNOP	"="
RELOP		">"|"<"|">="|"<="|"=="|"!="
PLUS		"+"
MINUS		"-"
STAR		"*"
DIV		"/"
AND		"&&"
OR		"||"
DOT		"."
NOT		"!"
TYPE		"int"|"float"
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

{Comment}	;
{MultiComment}	;
{WS}		;
{NEWLINE}	yycolumn = 1;

{SEMI}		{ yylval.s = strdup(yytext); return SEMI; }
{COMMA}		{ yylval.s = strdup(yytext); return COMMA; }
{ASSIGNOP}	{ yylval.s = strdup(yytext); return ASSIGNOP; }
{RELOP}		{ yylval.s = strdup(yytext); return RELOP; }
{PLUS}		{ yylval.s = strdup(yytext); return PLUS; }
{MINUS}		{ yylval.s = strdup(yytext); return MINUS; }
{STAR}		{ yylval.s = strdup(yytext); return STAR; }
{DIV}		{ yylval.s = strdup(yytext); return DIV; }
{AND}		{ yylval.s = strdup(yytext); return AND; }
{OR}		{ yylval.s = strdup(yytext); return OR; }
{DOT}		{ yylval.s = strdup(yytext); return DOT; }
{NOT}		{ yylval.s = strdup(yytext); return NOT; }
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
