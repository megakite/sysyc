#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "koopaext.h"

/* destructors */
static void free_value(koopa_raw_value_data_t *raw);
static void free_basic_block(koopa_raw_basic_block_data_t *basic_block);
static void free_function(koopa_raw_function_data_t *function);
static void free_type(koopa_raw_type_kind_t *type);

static void free_slice(koopa_raw_slice_t *slice)
{
	assert(slice);

	for (uint32_t i = 0; i < slice->len; ++i) {
		switch (slice->kind) {
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
}

inline static void free_slice_ref(koopa_raw_slice_t *slice)
{
	free(slice->buffer);
}

static void free_value(koopa_raw_value_data_t *raw)
{
	if (!raw)
		return;

	free_type(raw->ty);
	free(raw->name);
	free_slice_ref(&raw->used_by);

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
		assert(false /* unimplemented */);
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
	free_slice_ref(&raw->used_by);
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

/* public defn.s */
void koopa_delete_raw_program(koopa_raw_program_t *program)
{
	assert(program);

	free_slice(&program->values);
	free_slice(&program->funcs);
}
