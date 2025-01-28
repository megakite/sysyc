#ifndef _KOOPAEXT_H_
#define _KOOPAEXT_H_

#include "koopa.h"
#include "vector.h"

/* god i really don't have any clue how we can free() things with this crappy
 * cyclic, self-referenced tree structure. guess i'll just be lazy for a moment.
 * feel free to blame me if this is in fact a bad idea, i mean, storing
 * scattered pointers into two mere vectors. */
extern vector_t g_vec_functions;
extern vector_t g_vec_values;

/* slice operations */
void slice_append(koopa_raw_slice_t *slice, void *item);
koopa_raw_slice_t slice_new(uint32_t len, koopa_raw_slice_item_kind_t kind);
void slice_delete(koopa_raw_slice_t *slice);

/* destructor */
void koopa_delete_raw_program(koopa_raw_program_t *program);

#endif//_KOOPAEXT_H_
