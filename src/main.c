/**
 * main.c
 * The SysY compiler.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "debug.h"
#include "globals.h"
#include "ir.h"
#include "koopa.h"
#include "koopaext.h"
#include "macros.h"
#include "semantic.h"

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
	if (strcmp(mode, "-debug") == 0)
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

	/* parse */
	printf("======= Parsing...\n");
	yyin = fopen(input, "r");
	if (!yyin)
	{
		perror(input);
		return 1;
	}
	yyparse();
	fclose(yyin);

	/* check error from lexer/parser */
	if (error)
		return 1;

#if 0
	/* print AST */
	printf("======= Abstract syntax tree (AST)\n");
	ast_print(comp_unit);
#endif

	/* semantic analysis */
	if (setjmp(g_exception_env) == 0)
		semantic(comp_unit);
	else
		goto cleanup_comp_unit;

	/* generate memory IR */
	printf("======= Generating memory IR...\n");
	bump_t bump = bump_new(128 KiB);
	koopa_raw_program_set_allocator(bump);
	koopa_raw_program_t raw = ir(comp_unit);

	/* log memory IR */
	printf("======= Logging memory IR into stderr...\n");
	debug(&raw);

	/* try to convert memory IR into raw koopa program */
	printf("======= Verifying memory IR integrity...\n");
	koopa_program_t program;
	koopa_error_code_t ret = koopa_generate_raw_to_koopa(&raw, &program);
	if (ret != KOOPA_EC_SUCCESS)
	{
		printf("error code: %d", ret);
		goto cleanup_raw_program;
	}

	/* compile to RISC-V assembly */
	if (strcmp(mode, "-riscv") == 0)
	{
		/* dump IR */
		if (strcmp(middle, "-o") != 0)
		{
			printf("======= Dumping text-form Koopa IR...\n");
			koopa_dump_to_file(program, middle);
		}

		/* generate assembly */
		FILE *f = fopen(output, "w");
		printf("======= Generating assembly...\n");
		codegen(&raw, f);
		fclose(f);
	}

	/* generate Koopa IR */
	if (strcmp(mode, "-koopa") == 0)
	{
		/* dump IR */
		printf("======= Dumping text-form Koopa IR...\n");
		koopa_dump_to_file(program, output);
	}

	/* cleanup */
	printf("======= Cleaning up...\n");
	koopa_delete_program(program);
cleanup_raw_program:
	bump_delete(bump);
cleanup_comp_unit:
	node_delete(comp_unit);

	return 0;
}
