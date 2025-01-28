#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "koopaext.h"

/* exported slice operations */
void slice_append(koopa_raw_slice_t *slice, void *item)
{
	if (slice->len == 0)
	{
		slice->len = 1;
		slice->buffer = malloc(sizeof(void *));
		slice->buffer[0] = item;
	}
	else
	{
		/* i mean, this thing is definitely not designed for insertion,
		 * right? since it doesn't have a capacity field, thus we have
		 * to realloc() all the time.
		 * well, perhaps all of those _raw_ things aren't meant to be
		 * manipulated by human, and i'm just grinding some roadblocks.
		 * ah, nevermind. */
		++slice->len;
		slice->buffer = realloc(slice->buffer, 
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
		slice.buffer = malloc(sizeof(void *) * len);

	return slice;
}

inline void slice_delete(koopa_raw_slice_t *slice)
{
	free(slice->buffer);
	slice->len = 0;
}

/* deep destructors */
static void free_basic_block(koopa_raw_basic_block_data_t *basic_block);
static void free_function(koopa_raw_function_data_t *function);
static void free_type(koopa_raw_type_kind_t *type);
static void free_value(koopa_raw_value_data_t *raw);

void free_slice(koopa_raw_slice_t *slice)
{
	for (uint32_t i = 0; i < slice->len; ++i)
	{
		switch (slice->kind)
		{
		case KOOPA_RSIK_UNKNOWN:
			assert(false /* unknown slice type */);
			break;
		case KOOPA_RSIK_TYPE:
			free_type(slice->buffer[i]);
			break;
		case KOOPA_RSIK_FUNCTION:
			free_function(slice->buffer[i]);
			break;
		case KOOPA_RSIK_BASIC_BLOCK:
			free_basic_block(slice->buffer[i]);
			break;
		case KOOPA_RSIK_VALUE:
			free_value(slice->buffer[i]);
			break;
		}
	}
	free(slice->buffer);
	slice->len = 0;
}

static void free_value(koopa_raw_value_data_t *raw)
{
	if (!raw)
		return;

	free_type(raw->ty);
	free(raw->name);
	slice_delete(&raw->used_by);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
	case KOOPA_RVT_FUNC_ARG_REF:
	case KOOPA_RVT_BLOCK_ARG_REF:
		break;
	case KOOPA_RVT_AGGREGATE:
		free_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_ALLOC:
		assert(false /* todo */);
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		free_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		free_value(raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		free_value(raw->kind.data.store.value);
		free_value(raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		free_value(raw->kind.data.get_ptr.src);
		free_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		free_value(raw->kind.data.get_elem_ptr.src);
		free_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		free_value(raw->kind.data.binary.lhs);
		free_value(raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		free_value(raw->kind.data.branch.cond);
		free_basic_block(raw->kind.data.branch.true_bb);
		free_basic_block(raw->kind.data.branch.false_bb);
		free_slice(&raw->kind.data.branch.true_args);
		free_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		free_basic_block(raw->kind.data.jump.target);
		free_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		free_function(raw->kind.data.call.callee);
		free_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		free_value(raw->kind.data.ret.value);
		break;
	}

	free(raw);
}

static void free_basic_block(koopa_raw_basic_block_data_t *raw)
{
	if (!raw)
		return;

	free(raw->name);
	free_slice(&raw->params);
	slice_delete(&raw->used_by);
	free_slice(&raw->insts);

	free(raw);
}

static void free_function(koopa_raw_function_data_t *raw)
{
	if (!raw)
		return;

	free_type(raw->ty);
	free(raw->name);
	free_slice(&raw->params);
	free_slice(&raw->bbs);

	free(raw);
}

static void free_type(koopa_raw_type_kind_t *raw)
{
	if (!raw)
		return;

	switch (raw->tag) {
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		free_type(raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		free_type(raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		free_slice(&raw->data.function.params);
		free_type(raw->data.function.ret);
		break;	
	}

	free(raw);
}

/* shallow destructors which we're using right now */
static void basic_block_delete(koopa_raw_basic_block_data_t *basic_block);
static void function_delete(koopa_raw_function_data_t *function);
static void type_delete(koopa_raw_type_kind_t *type);
static void value_delete(koopa_raw_value_data_t *raw);

static void value_delete(koopa_raw_value_data_t *raw)
{
	if (!raw)
		return;

	type_delete(raw->ty);
	free(raw->name);
	slice_delete(&raw->used_by);

	free(raw);
}

static void basic_block_delete(koopa_raw_basic_block_data_t *raw)
{
	if (!raw)
		return;

	free(raw->name);
	slice_delete(&raw->params);
	slice_delete(&raw->used_by);
	slice_delete(&raw->insts);

	free(raw);
}

static void function_delete(koopa_raw_function_data_t *raw)
{
	if (!raw)
		return;

	type_delete(raw->ty);
	free(raw->name);
	slice_delete(&raw->params);
	slice_delete(&raw->bbs);

	free(raw);
}

static void type_delete(koopa_raw_type_kind_t *raw)
{
	if (!raw)
		return;

	switch (raw->tag) {
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		type_delete(raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		type_delete(raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		slice_delete(&raw->data.function.params);
		type_delete(raw->data.function.ret);
		break;	
	}

	free(raw);
}

/* public defn.s */
void koopa_delete_raw_program(koopa_raw_program_t *program)
{
	/* why aren't you casting this time? */
	void (*_function_delete)(void *) = (void (*)(void *))function_delete;
	void (*_value_delete)(void *) = (void (*)(void *))value_delete;

	assert(program);

	vector_iterate(g_vec_functions, _function_delete);
	vector_iterate(g_vec_values, _value_delete);

	slice_delete(&program->values);
	slice_delete(&program->funcs);
}
