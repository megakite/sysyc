#ifndef _MEMIR_H_
#define _MEMIR_H_

#include "koopa.h"
#include "node.h"

/**
 * Generate Koopa raw program from AST.
 * @return Generated raw program.
 */
koopa_raw_program_t memir_gen(struct node_t *program);

#endif//_MEMIR_H_
