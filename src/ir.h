#ifndef _IR_H_
#define _IR_H_

#include "koopa.h"
#include "node.h"
#include "bump.h"
#include "hashtable.h"

/* god i really don't have any clue how we can free() things with this crappy
 * cyclic, self-referenced tree (graph?) structure. guess i'll just be lazy for
 * a moment. feel free to blame me if this is in fact a bad idea. */
extern bump_t g_bump;
extern ht_strsym_t g_symbols;

/* Generate Koopa raw program from AST.
 * @return Generated raw program.
 */
koopa_raw_program_t ir(struct node_t *program);

#endif//_IR_H_
