#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hashtable.h"
#include "koopaext.h"
#include "macros.h"
#include "globals.h"

/* pool of `koopa_raw_type_t`s */
static htable_argptr_t m_raw_types;

/* raw program memory management */
void koopa_raw_program_set_allocator(bump_t bump)
{
	g_bump = bump;
}

/* slice operations */
void slice_append(koopa_raw_slice_t *slice, void *item)
{
	if (slice->len == 0)
	{
		slice->len = 1;
		slice->buffer = bump_malloc(g_bump, sizeof(void *));
		slice->buffer[0] = item;
	}
	else
	{
		/* i mean, this thing is definitely not designed for insertion,
		 * right? since it doesn't have a capacity field, thus we have
		 * to realloc() all the time.
		 * well, perhaps all of those _raw_ things aren't meant to be
		 * manipulated by human, you know, they're all const-qualified.
		 * ah, nevermind. */
		++slice->len;
		slice->buffer = bump_realloc(g_bump, slice->buffer, 
					     sizeof(void *) * slice->len);
		slice->buffer[slice->len - 1] = item;
	}
}

koopa_raw_slice_t slice_new(uint32_t len, koopa_raw_slice_item_kind_t kind)
{
	koopa_raw_slice_t slice = { .len = len, .kind = kind, };

	if (len == 0)
		slice.buffer = NULL;
	else
		slice.buffer = bump_malloc(g_bump, sizeof(void *) * len);

	return slice;
}

void slice_iter(koopa_raw_slice_t *slice, void (*fn)(void *))
{
	for (uint32_t i = 0; i < slice->len; ++i)
		fn(slice->buffer[i]);
}

/* IR builders */
koopa_raw_value_t koopa_raw_integer(int32_t value)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_INTEGER,
		.data.integer.value = value,
	};

	return ret;
}

koopa_raw_value_t koopa_raw_aggregate()
{
	unimplemented();
}

koopa_raw_value_t koopa_raw_func_arg_ref(size_t index)
{
	(void) index;
	todo();
}

koopa_raw_value_t koopa_raw_block_arg_ref(size_t index)
{
	(void) index;
	todo();
}

koopa_raw_value_t koopa_raw_global_alloc(char *name, koopa_raw_value_t init)
{
	(void) name, (void) init;
	todo();
}

koopa_raw_value_t koopa_raw_alloc(char *name, koopa_raw_type_t base)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_POINTER;
	ret_ty->data.pointer.base = base;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = name;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_ALLOC,
	};
	
	return ret;
}

koopa_raw_value_t koopa_raw_load(koopa_raw_value_t src)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_LOAD,
		.data.load = { .src = src, },
	};

	slice_append(&ret->kind.data.load.src->used_by, ret);

	return ret;
}

koopa_raw_value_t koopa_raw_store(koopa_raw_value_t value,
				  koopa_raw_value_t dest)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_UNIT;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_STORE,
		.data.store = { .value = value, .dest = dest, },
	};

	slice_append(&ret->kind.data.store.value->used_by, ret);
	slice_append(&ret->kind.data.store.dest->used_by, ret);

	return ret;
}

koopa_raw_value_t koopa_raw_get_ptr(koopa_raw_value_t src,
				    koopa_raw_value_t index)
{
	(void) src, (void) index;
	todo();
}

koopa_raw_value_t koopa_raw_get_elem_ptr(koopa_raw_value_t src,
					 koopa_raw_value_t index)
{
	(void) src, (void) index;
	todo();
}

koopa_raw_value_t koopa_raw_binary(koopa_raw_binary_op_t op,
				   koopa_raw_value_t lhs,
				   koopa_raw_value_t rhs)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_INT32;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_BINARY,
		.data.binary = { .op = op, .lhs = lhs, .rhs = rhs, },
	};

	slice_append(&ret->kind.data.binary.lhs->used_by, ret);
	slice_append(&ret->kind.data.binary.rhs->used_by, ret);

	return ret;
}

koopa_raw_value_t koopa_raw_branch(koopa_raw_value_t cond,
				   koopa_raw_basic_block_t true_bb,
				   koopa_raw_basic_block_t false_bb,
				   koopa_raw_slice_t true_args,
				   koopa_raw_slice_t false_args)
{
	(void) cond, (void) true_bb, (void) false_bb, (void) true_args,
		(void) false_args;
	todo();
}

koopa_raw_value_t koopa_raw_jump(koopa_raw_basic_block_t target,
				 koopa_raw_slice_t args)
{
	(void) target, (void) args;
	todo();
}

koopa_raw_value_t koopa_raw_call(koopa_raw_function_t callee,
				 koopa_raw_slice_t args)
{
	(void) callee, (void) args;
	todo();
}

koopa_raw_value_t koopa_raw_return(koopa_raw_value_t value)
{
	koopa_raw_type_kind_t *ret_ty = bump_malloc(g_bump, sizeof(*ret_ty));
	ret_ty->tag = KOOPA_RTT_UNIT;

	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ret_ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_RETURN,
		.data.ret = { .value = value, },
	};

	slice_append(&ret->kind.data.ret.value->used_by, ret);

	return ret;
}
