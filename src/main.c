#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "node.h"
#include "ast.h"
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
	const char *middle = argv[3];
	const char *output = argv[4];

	/* reference mode */
	if (strcmp(mode, "-reference") == 0)
	{
		koopa_program_t program;
		koopa_error_code_t ret = koopa_parse_from_file(middle,
							       &program);
		assert(ret == KOOPA_EC_SUCCESS);

		koopa_raw_program_builder_t builder =
			koopa_new_raw_program_builder();
		koopa_raw_program_t raw = koopa_build_raw_program(builder,
								  program);
		koopa_delete_program(program);

		debug(&raw);

		koopa_delete_raw_program_builder(builder);
		return 0;
	}

	/* compile to IR */
	if (strcmp(mode, "-koopa") == 0)
	{
		/* parse */
		yyin = fopen(input, "r");
		if (!yyin) {
			perror(input);
			return 1;
		}

		printf("======= Parsing...\n");
		yyparse();

		/* check error from lexer/parser */
		if (error) {
			return 1;
		}

		/* print out AST */
		printf("======= Abstract syntax tree (AST)\n");
		ast_print(comp_unit);

		/* generate memory IR */
		printf("======= Generating memory IR...\n");
		koopa_raw_program_t memir = memir_gen(comp_unit);

		/* log memory IR */
		printf("======= Logging memory IR into stderr...\n");
		debug(&memir);

		/* try to convert memory IR into raw koopa program */
		printf("======= Verifying memory IR integrity...\n");
		koopa_program_t program;
		koopa_error_code_t ret = koopa_generate_raw_to_koopa(&memir,
								     &program);
		assert(ret == KOOPA_EC_SUCCESS);

		/* dump IR */
		printf("======= Dumping text-form Koopa IR...\n");
		koopa_dump_to_file(program, output);

		/* cleanup */
		printf("======= Cleaning up...\n");
		koopa_delete_program(program);
		koopa_delete_raw_program(&memir);
		node_delete(comp_unit);

		return 0;
	}

	/* compile to RISC-V assembly */
	if (strcmp(mode, "-riscv") == 0)
	{
		/* parse */
		yyin = fopen(input, "r");
		if (!yyin) {
			perror(input);
			return 1;
		}

		printf("======= Parsing...\n");
		yyparse();

		/* check error from lexer/parser */
		if (error) {
			return 1;
		}

		/* print out AST */
		printf("======= Abstract syntax tree (AST)\n");
		ast_print(comp_unit);

		/* generate memory IR */
		printf("======= Generating memory IR...\n");
		koopa_raw_program_t memir = memir_gen(comp_unit);

		/* log memory IR */
		printf("======= Logging memory IR into stderr...\n");
		debug(&memir);

		/* try to convert memory IR into raw koopa program */
		printf("======= Verifying memory IR integrity...\n");
		koopa_program_t program;
		koopa_error_code_t ret = koopa_generate_raw_to_koopa(&memir,
								     &program);
		assert(ret == KOOPA_EC_SUCCESS);

		/* dump IR */
		printf("======= Dumping text-form Koopa IR...\n");
		koopa_dump_to_file(program, middle);

		/* generate assembly */
		FILE *f = fopen(output, "w");
		printf("======= Generating assembly...\n");
		codegen(&memir, f);
		fclose(f);

		/* cleanup */
		printf("======= Cleaning up...\n");
		koopa_delete_program(program);
		koopa_delete_raw_program(&memir);
		node_delete(comp_unit);

		return 0;
	}

	printf("usage: %s [-reference|-koopa|-riscv] <input> <middle> <output>"
	       "\n", argv[0]);
	return 1;
}
