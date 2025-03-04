#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "koopa.h"
#include "macros.h"

/* can't believe that __VA_OPT__ is a C23 feature... */
#define clog(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)

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

	clog("[ /* %d, &%s */ ", slice->len, ITEM_KIND_S[slice->kind]);
	for (uint32_t i = 0; i < slice->len; ++i)
		clog("%p, ", slice->buffer[i]);
	clog("], ");
}

static void raw_slice(const koopa_raw_slice_t *slice)
{
	assert(slice);

	clog("[ /* %d, %s */ ", slice->len, ITEM_KIND_S[slice->kind]);
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
	clog("], ");
}

void _debug_raw_program(const koopa_raw_program_t *program)
{
	if (!program)
	{
		clog("{}\n");
		return;
	}

	clog("{ ");

	clog("values: ");
	raw_slice(&program->values);
	clog("funcs: ");
	raw_slice(&program->funcs);

	clog("}\n");
}

/* accessor defn.s */
void _debug_raw_type(koopa_raw_type_t raw)
{
	if (!raw)
	{
		clog("{ /* Type */ }, ");
		return;
	}

	clog("{ /* Type %p */ ", raw);

	clog("tag: '%s', ", TYPE_TAG_S[raw->tag]);

	clog("data: { ");
	switch (raw->tag)
	{
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		clog("array: %p, ", raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		clog("pointer: %p, ", raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		clog("params: ");
		raw_slice(&raw->data.function.params);
		clog("function: %p, ", raw->data.function.ret);
		break;	
	}
	clog("}, ");

	clog("}, ");
}

void _debug_raw_function(koopa_raw_function_t raw)
{
	if (!raw)
	{
		clog("{ /* Function */ }, ");
		return;
	}

	clog("{ /* Function %p */ ", raw);

	clog("ty: ");
	_debug_raw_type(raw->ty);
	clog("name: '%s', ", raw->name);
	clog("params: ");
	raw_slice(&raw->params);
	clog("bbs: ");
	raw_slice(&raw->bbs);

	clog("}, ");
}

void _debug_raw_basic_block(koopa_raw_basic_block_t raw)
{
	if (!raw)
	{
		clog("{ /* Basic block */ }, ");
		return;
	}

	clog("{ /* Basic block %p */ ", raw);

	clog("name: '%s', ", raw->name);
	clog("params: ");
	raw_slice(&raw->params);
	clog("used_by: ");
	raw_slice_shallow(&raw->used_by);
	clog("insts: ");
	raw_slice(&raw->insts);

	clog("}, ");
}

/* TODO use this regex to replace with shallow visits
 * %s/clog("\(.*\): ");\n\t\t_debug_raw_.*(\(.*\));/LOG("\1: %p", \2); */
void _debug_raw_value(koopa_raw_value_t raw)
{
	if (!raw)
	{
		clog("{ /* Value */ }, ");
		return;
	}

	clog("{ /* Value %p */ ", raw);

	clog("ty: ");
	_debug_raw_type(raw->ty);
	clog("name: '%s', ", raw->name);
	clog("used_by: ");
	raw_slice_shallow(&raw->used_by);

#if 1
	/* shallow print */
	clog("kind: { ");
	clog("tag: '%s', ", VALUE_TAG_S[raw->kind.tag]);
	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		clog("value: %d, ", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		clog("elems: ");
		raw_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		clog("index: %zu, ",
			raw->kind.data.func_arg_ref.index);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		clog("index: %zu, ",
			raw->kind.data.block_arg_ref.index);
		break;
	case KOOPA_RVT_ALLOC:
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		clog("init: %p, ", raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		clog("src: %p, ", raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		clog("value: %p, ", raw->kind.data.store.value);
		clog("dest: %p, ", raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		clog("src: %p, ", raw->kind.data.get_ptr.src);
		clog("index: %p, ", raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		clog("src: %p, ", raw->kind.data.get_elem_ptr.src);
		clog("index: %p, ", raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		clog("op: '%s', ", BINARY_OP_S[raw->kind.data.binary.op]);
		clog("lhs: %p, ", raw->kind.data.binary.lhs);
		clog("rhs: %p, ", raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		clog("cond: %p, ", raw->kind.data.branch.cond);
		clog("true_bb: %p, ", raw->kind.data.branch.true_bb);
		clog("false_bb: %p, ", raw->kind.data.branch.false_bb);
		clog("true_args: ");
		raw_slice(&raw->kind.data.branch.true_args);
		clog("false_args: ");
		raw_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		clog("target: %p, ", raw->kind.data.jump.target);
		clog("args: ");
		raw_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		clog("callee: %p, ", raw->kind.data.call.callee);
		clog("args: ");
		raw_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		clog("value: %p, ", raw->kind.data.ret.value);
		break;
	}
	clog("}, ");

#else
        /* deep print */
	clog("kind: { ");
	clog("tag: '%s', ", VALUE_TAG_S[raw->kind.tag]);
	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		clog("value: %d, ", raw->kind.data.integer.value);
		break;
	case KOOPA_RVT_ZERO_INIT:
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		clog("elems: ");
		raw_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		clog("index: %zu, ", raw->kind.data.func_arg_ref.index);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		clog("index: %zu, ", raw->kind.data.block_arg_ref.index);
		break;
	case KOOPA_RVT_ALLOC:
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		clog("init: ");
		_debug_raw_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		clog("src: ");
		_debug_raw_value(raw->kind.data.load.src);
		break;
	case KOOPA_RVT_STORE:
		clog("value: ");
		_debug_raw_value(raw->kind.data.store.value);
		clog("dest: ");
		_debug_raw_value(raw->kind.data.store.dest);
		break;
	case KOOPA_RVT_GET_PTR:
		clog("src: ");
		_debug_raw_value(raw->kind.data.get_ptr.src);
		clog("index: ");
		_debug_raw_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		clog("src: ");
		_debug_raw_value(raw->kind.data.get_elem_ptr.src);
		clog("index: ");
		_debug_raw_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		clog("op: '%s', ", BINARY_OP_S[raw->kind.data.binary.op]);
		clog("lhs: ");
		_debug_raw_value(raw->kind.data.binary.lhs);
		clog("rhs: ");
		_debug_raw_value(raw->kind.data.binary.rhs);
		break;
	case KOOPA_RVT_BRANCH:
		clog("cond: ");
		_debug_raw_value(raw->kind.data.branch.cond);
		clog("true_bb: ");
		_debug_raw_basic_block(raw->kind.data.branch.true_bb);
		clog("false_bb: ");
		_debug_raw_basic_block(raw->kind.data.branch.false_bb);
		clog("true_args: ");
		raw_slice(&raw->kind.data.branch.true_args);
		clog("false_args: ");
		raw_slice(&raw->kind.data.branch.false_args);
		break;
	case KOOPA_RVT_JUMP:
		clog("target: ");
		_debug_raw_basic_block(raw->kind.data.jump.target);
		clog("args: ");
		raw_slice(&raw->kind.data.jump.args);
		break;
	case KOOPA_RVT_CALL:
		clog("callee: ");
		_debug_raw_function(raw->kind.data.call.callee);
		clog("args: ");
		raw_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		clog("value: ");
		_debug_raw_value(raw->kind.data.ret.value);
		break;
	}
	clog("}, ");
#endif

	clog("}, ");
}
