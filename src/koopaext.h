/**
 * koopaext.h
 * Extensions of `libkoopa` to make my life easier.
 */

#ifndef _KOOPAEXT_H_
#define _KOOPAEXT_H_

#include "koopa.h"
#include "bump.h"

/* raw program memory management */
void koopa_raw_program_set_allocator(bump_t bump);

/* slice operations */
koopa_raw_slice_t slice_new(uint32_t len, koopa_raw_slice_item_kind_t kind);
void slice_append(koopa_raw_slice_t *slice, void *item);
void slice_iter(koopa_raw_slice_t *slice, void (*fn)(void *));
void *slice_back(koopa_raw_slice_t *slice);

/* name constructors */
char *koopa_raw_name_global(char *ident);
char *koopa_raw_name_local(char *ident);

/* IR builders */
koopa_raw_value_t koopa_raw_integer(int32_t value);
koopa_raw_value_t koopa_raw_zero_init(koopa_raw_type_t ty);
koopa_raw_value_t koopa_raw_undef(koopa_raw_type_t ty);
koopa_raw_value_t koopa_raw_aggregate();
koopa_raw_value_t koopa_raw_func_arg_ref(char *name, size_t index);
koopa_raw_value_t koopa_raw_block_arg_ref(size_t index);
koopa_raw_value_t koopa_raw_alloc(char *name, koopa_raw_type_t base);
koopa_raw_value_t koopa_raw_global_alloc(char *name, koopa_raw_value_t init);
koopa_raw_value_t koopa_raw_load(koopa_raw_value_t src);
koopa_raw_value_t koopa_raw_store(koopa_raw_value_t value,
				  koopa_raw_value_t dest);
koopa_raw_value_t koopa_raw_get_ptr(koopa_raw_value_t src,
				    koopa_raw_value_t index);
koopa_raw_value_t koopa_raw_get_elem_ptr(koopa_raw_value_t src,
					 koopa_raw_value_t index);
koopa_raw_value_t koopa_raw_binary(koopa_raw_binary_op_t op,
				   koopa_raw_value_t lhs,
				   koopa_raw_value_t rhs);
koopa_raw_value_t koopa_raw_branch(koopa_raw_value_t cond,
				   koopa_raw_basic_block_t true_bb,
				   koopa_raw_basic_block_t false_bb);
koopa_raw_value_t koopa_raw_jump(koopa_raw_basic_block_t target);
koopa_raw_value_t koopa_raw_call(koopa_raw_function_t callee);
koopa_raw_value_t koopa_raw_return(koopa_raw_value_t value);

koopa_raw_type_t koopa_raw_type_int32(void);
koopa_raw_type_t koopa_raw_type_unit(void);
koopa_raw_type_t koopa_raw_type_array(koopa_raw_type_t base, size_t len);
koopa_raw_type_t koopa_raw_type_pointer(koopa_raw_type_t base);
koopa_raw_type_t koopa_raw_type_function(koopa_raw_type_t ret);

koopa_raw_function_t koopa_raw_function(koopa_raw_type_t ty, char *name);
koopa_raw_basic_block_t koopa_raw_basic_block(char *name);

#endif//_KOOPAEXT_H_
