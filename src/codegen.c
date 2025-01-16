#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "codegen.h"

#define EMIT(format, ...) fprintf(g_file, format __VA_OPT__(,) __VA_ARGS__)

/* state variables */
static FILE *g_file;

/* destructors */
static void codegen_value(koopa_raw_value_t raw);
static void codegen_basic_block(koopa_raw_basic_block_t basic_block);
static void codegen_function(koopa_raw_function_t function);
static void codegen_type(koopa_raw_type_t type);

static void codegen_slice(const koopa_raw_slice_t *slice)
{
	for (uint32_t i = 0; i < slice->len; ++i) {
		switch (slice->kind) {
		case KOOPA_RSIK_UNKNOWN:
			assert(false /* unknown slice type */);
			break;
		case KOOPA_RSIK_TYPE:
			codegen_type(slice->buffer[i]);
			break;
		case KOOPA_RSIK_FUNCTION:
			codegen_function(slice->buffer[i]);
			break;
		case KOOPA_RSIK_BASIC_BLOCK:
			codegen_basic_block(slice->buffer[i]);
			break;
		case KOOPA_RSIK_VALUE:
			codegen_value(slice->buffer[i]);
			break;
		}
	}
}

inline static void codegen_slice_ref(const koopa_raw_slice_t *slice)
{
}

static void codegen_value(koopa_raw_value_t raw)
{
	if (!raw)
		return;

	codegen_type(raw->ty);
	codegen_slice_ref(&raw->used_by);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		EMIT("%d", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_UNDEF:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_AGGREGATE:
		codegen_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_ALLOC:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		codegen_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		codegen_value(raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		codegen_value(raw->kind.data.store.value);
		codegen_value(raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		codegen_value(raw->kind.data.get_ptr.src);
		codegen_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		codegen_value(raw->kind.data.get_elem_ptr.src);
		codegen_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		codegen_value(raw->kind.data.binary.lhs);
		codegen_value(raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		codegen_value(raw->kind.data.branch.cond);
		codegen_basic_block(raw->kind.data.branch.true_bb);
		codegen_basic_block(raw->kind.data.branch.false_bb);
		codegen_slice(&raw->kind.data.branch.true_args);
		codegen_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		codegen_basic_block(raw->kind.data.jump.target);
		codegen_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		codegen_function(raw->kind.data.call.callee);
		codegen_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		EMIT("  li a0, ");
		codegen_value(raw->kind.data.ret.value);
		EMIT("\n");
		EMIT("  ret\n");
		break;
	}
}

static void codegen_basic_block(koopa_raw_basic_block_t raw)
{
	if (!raw)
		return;

	codegen_slice(&raw->params);
	codegen_slice_ref(&raw->used_by);
	codegen_slice(&raw->insts);
}

static void codegen_function(koopa_raw_function_t raw)
{
	if (!raw)
		return;

	if (strcmp(raw->name, "@main") == 0)
		EMIT("  .globl main\n");
	EMIT("%s:\n", raw->name + 1);

	codegen_type(raw->ty);
	codegen_slice(&raw->params);
	codegen_slice(&raw->bbs);
}

static void codegen_type(koopa_raw_type_t raw)
{
	if (!raw)
		return;

	switch (raw->tag) {
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		codegen_type(raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		codegen_type(raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		codegen_slice(&raw->data.function.params);
		codegen_type(raw->data.function.ret);
		break;	
	}
}

/* public defn.s */
void codegen(const koopa_raw_program_t *program, FILE *file)
{
	assert(file);
	g_file = file;

	/* TODO: support data segment */
	codegen_slice(&program->values);
	EMIT("  .text\n");
	codegen_slice(&program->funcs);
}
