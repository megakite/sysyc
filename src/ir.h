/**
 * ir.h
 * Koopa IR generation module.
 */

#ifndef _IR_H_
#define _IR_H_

#include "koopa.h"
#include "node.h"
#include "bump.h"
#include "hashtable.h"

/* Generate Koopa raw program from AST.
 * @return Generated raw program. */
koopa_raw_program_t ir(const struct node_t *program);

#endif//_IR_H_
