#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "koopa.h"
#include "macros.h"

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
static void raw_slice_shallow(const koopa_raw_slice_t *slice)
{
	assert(slice);

	dbg("[ /* %d, &%s */ ", slice->len, ITEM_KIND_S[slice->kind]);
	for (uint32_t i = 0; i < slice->len; ++i)
		dbg("%p, ", slice->buffer[i]);
	dbg("], ");
}

static void raw_slice(const koopa_raw_slice_t *slice)
{
	assert(slice);

	dbg("[ /* %d, %s */ ", slice->len, ITEM_KIND_S[slice->kind]);
	for (uint32_t i = 0; i < slice->len; ++i)
	{
		switch (slice->kind)
		{
		case KOOPA_RSIK_UNKNOWN:
			panic("unknown slice type");
			break;
		case KOOPA_RSIK_TYPE:
			_debug_raw_type(slice->buffer[i]);
			break;
		case KOOPA_RSIK_FUNCTION:
			_debug_raw_function(slice->buffer[i]);
			break;
		case KOOPA_RSIK_BASIC_BLOCK:
			_debug_raw_basic_block(slice->buffer[i]);
			break;
		case KOOPA_RSIK_VALUE:
			_debug_raw_value(slice->buffer[i]);
			break;
		}
	}
	dbg("], ");
}

void _debug_raw_program(const koopa_raw_program_t *program)
{
	if (!program)
	{
		dbg("{}\n");
		return;
	}

	dbg("{ ");

	dbg("values: ");
	raw_slice(&program->values);
	dbg("funcs: ");
	raw_slice(&program->funcs);

	dbg("}\n");
}

/* accessor defn.s */
void _debug_raw_type(koopa_raw_type_t raw)
{
	if (!raw)
	{
		dbg("{ /* Type */ }, ");
		return;
	}

	dbg("{ /* Type %p */ ", raw);

	dbg("tag: '%s', ", TYPE_TAG_S[raw->tag]);

	dbg("data: { ");
	switch (raw->tag)
	{
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		dbg("array: %p, ", raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		dbg("pointer: %p, ", raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		dbg("params: ");
		raw_slice(&raw->data.function.params);
		dbg("function: %p, ", raw->data.function.ret);
		break;	
	}
	dbg("}, ");

	dbg("}, ");
}

void _debug_raw_function(koopa_raw_function_t raw)
{
	if (!raw)
	{
		dbg("{ /* Function */ }, ");
		return;
	}

	dbg("{ /* Function %p */ ", raw);

	dbg("ty: ");
	_debug_raw_type(raw->ty);
	dbg("name: '%s', ", raw->name);
	dbg("params: ");
	raw_slice(&raw->params);
	dbg("bbs: ");
	raw_slice(&raw->bbs);

	dbg("}, ");
}

void _debug_raw_basic_block(koopa_raw_basic_block_t raw)
{
	if (!raw)
	{
		dbg("{ /* Basic block */ }, ");
		return;
	}

	dbg("{ /* Basic block %p */ ", raw);

	dbg("name: '%s', ", raw->name);
	dbg("params: ");
	raw_slice(&raw->params);
	dbg("used_by: ");
	raw_slice_shallow(&raw->used_by);
	dbg("insts: ");
	raw_slice(&raw->insts);

	dbg("}, ");
}

void _debug_raw_value(koopa_raw_value_t raw)
{
	if (!raw)
	{
		dbg("{ /* Value */ }, ");
		return;
	}

	dbg("{ /* Value %p */ ", raw);

	dbg("ty: ");
	_debug_raw_type(raw->ty);
	dbg("name: '%s', ", raw->name);
	dbg("used_by: ");
	raw_slice_shallow(&raw->used_by);

#if 1
	/* shallow print */
	dbg("kind: { ");
	dbg("tag: '%s', ", VALUE_TAG_S[raw->kind.tag]);
	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		dbg("value: %d, ", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		dbg("elems: ");
		raw_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		dbg("index: %zu, ", raw->kind.data.func_arg_ref.index);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		dbg("index: %zu, ", raw->kind.data.block_arg_ref.index);
		break;
	case KOOPA_RVT_ALLOC:
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		dbg("init: %p, ", raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		dbg("src: %p, ", raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		dbg("value: %p, ", raw->kind.data.store.value);
		dbg("dest: %p, ", raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		dbg("src: %p, ", raw->kind.data.get_ptr.src);
		dbg("index: %p, ", raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		dbg("src: %p, ", raw->kind.data.get_elem_ptr.src);
		dbg("index: %p, ", raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		dbg("op: '%s', ", BINARY_OP_S[raw->kind.data.binary.op]);
		dbg("lhs: %p, ", raw->kind.data.binary.lhs);
		dbg("rhs: %p, ", raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		dbg("cond: %p, ", raw->kind.data.branch.cond);
		dbg("true_bb: %p, ", raw->kind.data.branch.true_bb);
		dbg("false_bb: %p, ", raw->kind.data.branch.false_bb);
		dbg("true_args: ");
		raw_slice(&raw->kind.data.branch.true_args);
		dbg("false_args: ");
		raw_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		dbg("target: %p, ", raw->kind.data.jump.target);
		dbg("args: ");
		raw_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		dbg("callee: %p, ", raw->kind.data.call.callee);
		dbg("args: ");
		raw_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		if (raw->kind.data.ret.value)
			dbg("value: %p, ", raw->kind.data.ret.value);
		else
			dbg("value: null, ");
		break;
	}
	dbg("}, ");

#else
        /* deep print */
	dbg("kind: { ");
	dbg("tag: '%s', ", VALUE_TAG_S[raw->kind.tag]);
	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		dbg("value: %d, ", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		dbg("elems: ");
		raw_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		dbg("index: %zu, ", raw->kind.data.func_arg_ref.index);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		dbg("index: %zu, ", raw->kind.data.block_arg_ref.index);
		break;
	case KOOPA_RVT_ALLOC:
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		dbg("init: ");
		_debug_raw_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		dbg("src: ");
		_debug_raw_value(raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		dbg("value: ");
		_debug_raw_value(raw->kind.data.store.value);
		dbg("dest: ");
		_debug_raw_value(raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		dbg("src: ");
		_debug_raw_value(raw->kind.data.get_ptr.src);
		dbg("index: ");
		_debug_raw_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		dbg("src: ");
		_debug_raw_value(raw->kind.data.get_elem_ptr.src);
		dbg("index: ");
		_debug_raw_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		dbg("op: '%s', ", BINARY_OP_S[raw->kind.data.binary.op]);
		dbg("lhs: ");
		_debug_raw_value(raw->kind.data.binary.lhs);
		dbg("rhs: ");
		_debug_raw_value(raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		dbg("cond: ");
		_debug_raw_value(raw->kind.data.branch.cond);
		dbg("true_bb: ");
		_debug_raw_basic_block(raw->kind.data.branch.true_bb);
		dbg("false_bb: ");
		_debug_raw_basic_block(raw->kind.data.branch.false_bb);
		dbg("true_args: ");
		raw_slice(&raw->kind.data.branch.true_args);
		dbg("false_args: ");
		raw_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		dbg("target: ");
		_debug_raw_basic_block(raw->kind.data.jump.target);
		dbg("args: ");
		raw_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		dbg("callee: ");
		_debug_raw_function(raw->kind.data.call.callee);
		dbg("args: ");
		raw_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		dbg("value: ");
		_debug_raw_value(raw->kind.data.ret.value);
		break;
	}
	dbg("}, ");
#endif

	dbg("}, ");
}
