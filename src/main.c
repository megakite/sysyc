#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "node.h"
#include "ast.h"
#include "irgen.h"
#include "debug.h"
#include "memir.h"
#include "koopaext.h"
#include "codegen.h"

#include "koopa.h"

/* yacc variables */
extern FILE *yyin;
extern int yyparse(void);

/* state variables */
extern bool error;
extern struct node_t *comp_unit;

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

	if (error) {
		return 1;
	}

#if 0
	/* generate string IR */
	const char *ir = irgen(comp_unit);

	/* write to file */
	FILE *f = fopen(output, "w");
	fputs(ir, f);
	fclose(f);

	/* print AST and IR */
	printf("======= AST:\n");
	ast_print(comp_unit);
	printf("======= IR:\n");
	puts(ir);

	/* convert string IR to memory IR */
	koopa_program_t program;
	koopa_error_code_t ret = koopa_parse_from_string(ir, &program);
	assert(ret == KOOPA_EC_SUCCESS);
	koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
	koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
	koopa_delete_program(program);
	
	/* log IR to stderr */
	debug_koopa(&raw);
	printf("======= Reference memory IR has been logged to stderr.\n");

	koopa_delete_raw_program_builder(builder);
	free(ir);
#else
	/* generate memory IR */
	koopa_raw_program_t memir = memir_gen(comp_unit);

	/* try to convert memory IR into raw koopa program */
	koopa_program_t program;
	koopa_error_code_t ret = koopa_generate_raw_to_koopa(&memir, &program);
	assert(ret == KOOPA_EC_SUCCESS);

	/* log memory IR */
	debug(&memir);
	printf("======= Memory IR has been logged to stderr.\n");
	
	/* generate assembly code */
	FILE *f = fopen(output, "w");
	codegen(&memir, f);
	fclose(f);

	koopa_delete_program(program);
	koopa_delete_raw_program(&memir);
#endif

	node_destroy(comp_unit);
	return 0;
}
