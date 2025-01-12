#include <stdio.h>
#include <stdbool.h>

#include "node.h"
#include "ast.h"
#include "irgen.h"

/* yacc variables */
extern FILE *yyin;
extern int yyparse(void);

/* state variables */
extern bool error;
extern struct node_t *program;

int main(int argc, char **argv)
{
	if (argc != 5)
		return 1;

	const char *mode = argv[1];
	const char *input = argv[2];
	const char *output = argv[4];

	yyin = fopen(input, "r");
	if (!yyin) {
		perror(input);
		return 1;
	}
	yyparse();

	if (!error)
		ast_print(program);
	char *ir = irgen(program);
	puts(ir);

	FILE *f = fopen(output, "w");
	fputs(ir, f);
	fclose(f);

	free(ir);
	node_destroy(program);
	return 0;
}
