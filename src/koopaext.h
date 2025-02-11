#ifndef _KOOPAEXT_H_
#define _KOOPAEXT_H_

#include "koopa.h"
#include "bump.h"

/* slice operations */
void slice_append(koopa_raw_slice_t *slice, void *item);
koopa_raw_slice_t slice_new(uint32_t len, koopa_raw_slice_item_kind_t kind);
void slice_iter(koopa_raw_slice_t *slice, void (*fn)(void *));

/* raw program memory management */
void koopa_raw_program_set_allocator(bump_t bump);

#endif//_KOOPAEXT_H_
