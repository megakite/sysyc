#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "debug.h"
#include "hashtable.h"
#include "koopaext.h"
#include "macros.h"
#include "globals.h"

// TODO make a table that efficiently deduplicates different kinds of values
// that are in fact the same thing

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
		slice->buffer = bump_realloc(g_bump, slice->buffer, 
					     sizeof(void *) * ++slice->len);
		slice->buffer[slice->len - 1] = item;
	}
}

void *slice_back(koopa_raw_slice_t *slice)
{
	if (slice->len == 0)
		return NULL;
	
	return slice->buffer[slice->len - 1];
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

/* name constructors */
char *koopa_raw_name_global(char *ident)
{
	if (!ident)
		return NULL;

	char name[1 + IDENT_MAX];
	snprintf(name, 1 + IDENT_MAX, "@%s", ident);
	name[IDENT_MAX] = '\0';

	return bump_strdup(g_bump, name);
}

char *koopa_raw_name_local(char *ident)
{
	if (!ident)
		return NULL;

	char name[1 + IDENT_MAX];
	snprintf(name, 1 + IDENT_MAX, "%%%s", ident);
	name[IDENT_MAX] = '\0';

	return bump_strdup(g_bump, name);
}

/* common objects */
static koopa_raw_type_kind_t KOOPA_RAW_TYPE_KIND_INT32 = {
	.tag = KOOPA_RTT_INT32,
};

static koopa_raw_type_kind_t KOOPA_RAW_TYPE_KIND_UNIT = {
	.tag = KOOPA_RTT_UNIT,
};

/* IR builders */
koopa_raw_type_t koopa_raw_type_int32(void)
{
	return &KOOPA_RAW_TYPE_KIND_INT32;
}

koopa_raw_type_t koopa_raw_type_unit(void)
{
	return &KOOPA_RAW_TYPE_KIND_UNIT;
}

koopa_raw_type_t koopa_raw_type_array(koopa_raw_type_t base, size_t len)
{
	koopa_raw_type_kind_t *type = bump_malloc(g_bump, sizeof(*type));
	type->tag = KOOPA_RTT_ARRAY;
	type->data.array.base = base;
	type->data.array.len = len;

	return type;
}

koopa_raw_type_t koopa_raw_type_pointer(koopa_raw_type_t base)
{
	koopa_raw_type_kind_t *type = bump_malloc(g_bump, sizeof(*type));
	type->tag = KOOPA_RTT_POINTER;
	type->data.pointer.base = base;

	return type;
}

koopa_raw_type_t koopa_raw_type_function(koopa_raw_type_t ret)
{
	koopa_raw_type_kind_t *type = bump_malloc(g_bump, sizeof(*type));
	type->tag = KOOPA_RTT_FUNCTION;
	type->data.function.params = slice_new(0, KOOPA_RSIK_TYPE);
	type->data.function.ret = ret;

	return type;
}

koopa_raw_value_t koopa_raw_integer(int32_t value)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_int32();
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

koopa_raw_value_t koopa_raw_zero_init(koopa_raw_type_t ty)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ty;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_ZERO_INIT,
	};

	return ret;
}

koopa_raw_value_t koopa_raw_undef(koopa_raw_type_t ty)
{
	(void) ty;
	todo();
}

koopa_raw_value_t koopa_raw_func_arg_ref(char *name, size_t index)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_int32();
	ret->name = name;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_FUNC_ARG_REF,
		.data.func_arg_ref.index = index,
	};

	return ret;
}

koopa_raw_value_t koopa_raw_block_arg_ref(size_t index)
{
	(void) index;
	todo();
}

koopa_raw_value_t koopa_raw_global_alloc(char *name, koopa_raw_value_t init)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_pointer(init->ty);
	ret->name = name;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_GLOBAL_ALLOC,
		.data.global_alloc = { .init = init, },
	};

	slice_append(&ret->kind.data.global_alloc.init->used_by, ret);
	
	return ret;
}

koopa_raw_value_t koopa_raw_alloc(char *name, koopa_raw_type_t base)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_pointer(base);
	ret->name = name;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_ALLOC,
	};
	
	return ret;
}

koopa_raw_value_t koopa_raw_load(koopa_raw_value_t src)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_int32();
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
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_unit();
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
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_int32();
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
				   koopa_raw_basic_block_t false_bb)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_unit();
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_BRANCH,
		.data.branch = {
			.cond = cond,
			.true_bb = true_bb,
			.false_bb = false_bb,
			.true_args = slice_new(0, KOOPA_RSIK_VALUE),
			.false_args = slice_new(0, KOOPA_RSIK_VALUE),
		},
	};

	slice_append(&ret->kind.data.branch.cond->used_by, ret);
	slice_append(&ret->kind.data.branch.true_bb->used_by, ret);
	slice_append(&ret->kind.data.branch.false_bb->used_by, ret);

	return ret;
}

koopa_raw_value_t koopa_raw_jump(koopa_raw_basic_block_t target)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_unit();
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_JUMP,
		.data.jump = {
			.target = target,
			.args = slice_new(0, KOOPA_RSIK_VALUE),
		},
	};

	slice_append(&ret->kind.data.jump.target->used_by, ret);

	return ret;
}

koopa_raw_value_t koopa_raw_call(koopa_raw_function_t callee)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = callee->ty->data.function.ret;
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_CALL,
		.data.call = {
			.callee = callee,
			.args = slice_new(0, KOOPA_RSIK_VALUE),
		},
	};

	return ret;
}

koopa_raw_value_t koopa_raw_return(koopa_raw_value_t value)
{
	koopa_raw_value_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = koopa_raw_type_unit();
	ret->name = NULL;
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->kind = (koopa_raw_value_kind_t) {
		.tag = KOOPA_RVT_RETURN,
		.data.ret = { .value = value, },
	};

	/* we can in fact return nothing */
	if (value != NULL)
		slice_append(&ret->kind.data.ret.value->used_by, ret);

	return ret;
}

koopa_raw_basic_block_t koopa_raw_basic_block(char *name)
{
	koopa_raw_basic_block_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->name = koopa_raw_name_local(name);
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->used_by = slice_new(0, KOOPA_RSIK_VALUE);
	ret->insts = slice_new(0, KOOPA_RSIK_VALUE);

	return ret;
}

koopa_raw_function_t koopa_raw_function(koopa_raw_type_t ty, char *name)
{
	koopa_raw_function_data_t *ret = bump_malloc(g_bump, sizeof(*ret));
	ret->ty = ty;
	ret->name = koopa_raw_name_global(name);
	ret->params = slice_new(0, KOOPA_RSIK_VALUE);
	ret->bbs = slice_new(0, KOOPA_RSIK_BASIC_BLOCK);

	return ret;
}
