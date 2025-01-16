#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "koopa.h"

// can't believe that __VA_OPT__ is a C23 feature...
#define LOG(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)

/* enum strings */
static const char *ITEM_KIND_S[] = {
	"KOOPA_RSIK_UNKNOWN",
	"KOOPA_RSIK_TYPE",
	"KOOPA_RSIK_FUNCTION",
	"KOOPA_RSIK_BASIC_BLOCK",
	"KOOPA_RSIK_VALUE",
};

static const char *TYPE_TAG_S[] = {
	"KOOPA_RTT_INT32",
	"KOOPA_RTT_UNIT",
	"KOOPA_RTT_ARRAY",
	"KOOPA_RTT_POINTER",
	"KOOPA_RTT_FUNCTION",
};

static const char *BINARY_OP_S[] = {
	"KOOPA_RBO_NOT_EQ",
	"KOOPA_RBO_EQ",
	"KOOPA_RBO_GT",
	"KOOPA_RBO_LT",
	"KOOPA_RBO_GE",
	"KOOPA_RBO_LE",
	"KOOPA_RBO_ADD",
	"KOOPA_RBO_SUB",
	"KOOPA_RBO_MUL",
	"KOOPA_RBO_DIV",
	"KOOPA_RBO_MOD",
	"KOOPA_RBO_AND",
	"KOOPA_RBO_OR",
	"KOOPA_RBO_XOR",
	"KOOPA_RBO_SHL",
	"KOOPA_RBO_SHR",
	"KOOPA_RBO_SAR",
};

static const char* VALUE_TAG_S[] = {
	"KOOPA_RVT_INTEGER",
	"KOOPA_RVT_ZERO_INIT",
	"KOOPA_RVT_UNDEF",
	"KOOPA_RVT_AGGREGATE",
	"KOOPA_RVT_FUNC_ARG_REF",
	"KOOPA_RVT_BLOCK_ARG_REF",
	"KOOPA_RVT_ALLOC",
	"KOOPA_RVT_GLOBAL_ALLOC",
	"KOOPA_RVT_LOAD",
	"KOOPA_RVT_STORE",
	"KOOPA_RVT_GET_PTR",
	"KOOPA_RVT_GET_ELEM_PTR",
	"KOOPA_RVT_BINARY",
	"KOOPA_RVT_BRANCH",
	"KOOPA_RVT_JUMP",
	"KOOPA_RVT_CALL",
	"KOOPA_RVT_RETURN",
};

/* tool functions */
static void debug_slice_shallow(const koopa_raw_slice_t *slice)
{
	assert(slice);

	LOG("[ /* %d, &%s */ ", slice->len, ITEM_KIND_S[slice->kind]);
	for (uint32_t i = 0; i < slice->len; ++i) {
		LOG("%p, ", slice->buffer[i]);
	}
	LOG("], ");
}

static void debug_slice(const koopa_raw_slice_t *slice)
{
	assert(slice);

	LOG("[ /* %d, %s */ ", slice->len, ITEM_KIND_S[slice->kind]);
	for (uint32_t i = 0; i < slice->len; ++i) {
		switch (slice->kind) {
		case KOOPA_RSIK_UNKNOWN:
			assert(false /* unknown slice type */);
			break;
		case KOOPA_RSIK_TYPE:
			_debug_type(slice->buffer[i]);
			break;
		case KOOPA_RSIK_FUNCTION:
			_debug_function(slice->buffer[i]);
			break;
		case KOOPA_RSIK_BASIC_BLOCK:
			_debug_basic_block(slice->buffer[i]);
			break;
		case KOOPA_RSIK_VALUE:
			_debug_value(slice->buffer[i]);
			break;
		}
	}
	LOG("], ");
}

void _debug_program(const koopa_raw_program_t *program)
{
	if (!program) {
		LOG("{}\n");
		return;
	}

	LOG("{ ");

	LOG("values: ");
	debug_slice(&program->values);
	LOG("funcs: ");
	debug_slice(&program->funcs);

	LOG("}\n");
}

/* accessor defn.s */
void _debug_type(koopa_raw_type_t raw)
{
	if (!raw) {
		LOG("{ /* Type */ }, ");
		return;
	}

	LOG("{ /* Type */ ");

	LOG("tag: '%s', ", TYPE_TAG_S[raw->tag]);

	LOG("data: { ");
	switch (raw->tag) {
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		LOG("array: ");
		_debug_type(raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		LOG("pointer: ");
		_debug_type(raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		LOG("params: ");
		debug_slice(&raw->data.function.params);
		LOG("function: ");
		_debug_type(raw->data.function.ret);
		break;	
	}
	LOG("}, ");

	LOG("}, ");
}

void _debug_function(koopa_raw_function_t raw)
{
	if (!raw) {
		LOG("{ /* Function */ }, ");
		return;
	}

	LOG("{ /* Function */ ");

	LOG("ty: ");
	_debug_type(raw->ty);
	LOG("name: '%s', ", raw->name);
	LOG("params: ");
	debug_slice(&raw->params);
	LOG("bbs: ");
	debug_slice(&raw->bbs);

	LOG("}, ");
}

void _debug_basic_block(koopa_raw_basic_block_t raw)
{
	if (!raw) {
		LOG("{ /* Basic block */ }, ");
		return;
	}

	LOG("{ /* Basic block at %p */ ", raw);

	LOG("name: '%s', ", raw->name);
	LOG("params: ");
	debug_slice(&raw->params);
	LOG("used_by: ");
	debug_slice_shallow(&raw->used_by);
	LOG("insts: ");
	debug_slice(&raw->insts);

	LOG("}, ");
}

void _debug_value(koopa_raw_value_t raw)
{
	if (!raw) {
		LOG("{ /* Value */ }, ");
		return;
	}

	LOG("{ /* Value at %p */ ", raw);

	LOG("ty: ");
	_debug_type(raw->ty);
	LOG("name: '%s', ", raw->name);
	LOG("used_by: ");
	debug_slice_shallow(&raw->used_by);

	LOG("kind: { ");
	LOG("tag: '%s', ", VALUE_TAG_S[raw->kind.tag]);
	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		LOG("value: %d, ", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_AGGREGATE:
		LOG("elems: ");
		debug_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		LOG("index: %zu, ",
			raw->kind.data.func_arg_ref.index);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		LOG("index: %zu, ",
			raw->kind.data.block_arg_ref.index);
		break;
	case KOOPA_RVT_ALLOC:
		assert(false /* unimplemented */);
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		LOG("init: ");
		_debug_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		LOG("src: ");
		_debug_value(raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		LOG("value: ");
		_debug_value(raw->kind.data.store.value);
		LOG("dest: ");
		_debug_value(raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		LOG("src: ");
		_debug_value(raw->kind.data.get_ptr.src);
		LOG("index: ");
		_debug_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		LOG("src: ");
		_debug_value(raw->kind.data.get_elem_ptr.src);
		LOG("index: ");
		_debug_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		LOG("op: %d, ", raw->kind.data.binary.op);
		LOG("lhs: ");
		_debug_value(raw->kind.data.binary.lhs);
		LOG("rhs: ");
		_debug_value(raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		LOG("cond: ");
		_debug_value(raw->kind.data.branch.cond);
		LOG("true_bb: ");
		_debug_basic_block(raw->kind.data.branch.true_bb);
		LOG("false_bb: ");
		_debug_basic_block(raw->kind.data.branch.false_bb);
		LOG("true_args: ");
		debug_slice(&raw->kind.data.branch.true_args);
		LOG("false_args: ");
		debug_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		LOG("target: ");
		_debug_basic_block(raw->kind.data.jump.target);
		LOG("args: ");
		debug_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		LOG("callee: ");
		_debug_function(raw->kind.data.call.callee);
		LOG("args: ");
		debug_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		LOG("value: ");
		_debug_value(raw->kind.data.ret.value);
		break;
	}
	LOG("}, ");

	LOG("}, ");
}
