#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hashtable.h"
#include "codegen.h"
#include "koopaext.h"

/* Optional<Either<ValuePtr, ValueIndex>> */
struct variant_t {
	enum {
		NIL = 0,
		VALUE,
		INDEX,
		OFFSET,
	} tag;
	union {
		koopa_raw_value_t value;
		uint32_t index;
		uint32_t offset;
	};
};

/* state variables */
static FILE *m_output;
static ht_ptru32_t m_ht_insts;
static ht_ptru32_t m_ht_offsets;
static uint32_t m_inst_idx;
static uint8_t m_reg_t_idx;
static int m_stack_offset;

/* emitter */
#define EMIT(format, ...) fprintf(m_output, format __VA_OPT__(,) __VA_ARGS__)

static void OPER(const char *op, unsigned times)
{
	assert(times >= 0 && times <= 3);

	switch (times)
	{
	case 0:
		/* dedicated for `sw` */
		EMIT("  %s ", op);
		break;
	case 1:
		/* one output */
		if (m_inst_idx < 7)
			EMIT("  %s t%d, ", op, m_inst_idx);
		else
			EMIT("  %s a%d, ", op, m_inst_idx - 7);
		break;
	case 2:
		/* self repeating */
		if (m_inst_idx < 7)
			EMIT("  %s t%d, t%d\n", op, m_inst_idx, m_inst_idx);
		else
			EMIT("  %s a%d, a%d\n", op, m_inst_idx - 7,
			     m_inst_idx - 7);
		break;
	case 3:
		if (m_inst_idx < 7)
			EMIT("  %s t%d, t%d, t%d\n", op, m_inst_idx, m_inst_idx,
			     m_inst_idx);
		else
			EMIT("  %s a%d, a%d, a%d\n", op, m_inst_idx - 7,
			     m_inst_idx - 7, m_inst_idx - 7);
		break;
	}
}

static void OPND(struct variant_t *variant, bool newline)
{
	switch (variant->tag)
	{
	case VALUE:
		if (variant->value->kind.tag == KOOPA_RVT_INTEGER &&
		    variant->value->kind.data.integer.value == 0)
			EMIT(newline ? "x0\n" : "x0, ");
		else
		{
			if (--m_reg_t_idx < 7)
				EMIT(newline ? "t%d\n" : "t%d, ", m_reg_t_idx);
			else
				EMIT(newline ? "a%d\n" : "a%d, ",
				     m_reg_t_idx - 7);
		}
		break;
	case INDEX:
		if (variant->index < 7)
			EMIT(newline ? "t%d\n" : "t%d, ", variant->index);
		else
			EMIT(newline ? "a%d\n" : "a%d, ", variant->index - 7);
		break;
	case OFFSET:
		EMIT(newline ? "%d(sp)\n" : "%d(sp)", variant->offset);
		break;
	default:
		assert(false /* missing value */);
	}
}

/* declarations */
static struct variant_t codegen_value(koopa_raw_value_t raw);
static void codegen_basic_block(koopa_raw_basic_block_t basic_block);
static void codegen_function(koopa_raw_function_t function);
static void codegen_type(koopa_raw_type_t type);

/* utilities */
static void reserve_stack(void *block)
{
	koopa_raw_basic_block_t _block = block;
	for (uint32_t i = 0; i < _block->insts.len; ++i)
	{
		koopa_raw_value_t value = _block->insts.buffer[i];
		if (value->ty->tag != KOOPA_RTT_POINTER)
			continue;

		// it's so sad that C doesn't have closures...which means i need
		// to declare another member to record the offset.
		ht_ptru32_upsert(m_ht_offsets, value, -m_stack_offset);
		m_stack_offset -= sizeof(int32_t);
	}
}

/* allocator */
static void function_prologue(koopa_raw_function_t function)
{
	slice_iter(&function->bbs, reserve_stack);
	// align to 16 bytes
	m_stack_offset &= -16;
	EMIT("  addi sp, sp, %d\n", m_stack_offset);
}

static void function_epilogue(void)
{
	EMIT("  addi sp, sp, %d\n", -m_stack_offset);
	// reset stack offset
	m_stack_offset = 0;
}

/* kind generators */
static void codegen_kind_load(koopa_raw_load_t *load)
{
	struct variant_t src = codegen_value(load->src);

	OPER("lw", 1);
	OPND(&src, true);
}

static void codegen_kind_store(koopa_raw_store_t *store)
{
	struct variant_t value = codegen_value(store->value);
	struct variant_t dest = codegen_value(store->dest);

	OPER("sw", 0);
	OPND(&value, false);
	OPND(&dest, true);
}

static void codegen_kind_return(koopa_raw_return_t *ret)
{
	struct variant_t value = codegen_value(ret->value);

	EMIT("  mv a0, ");
	OPND(&value, true);
	function_epilogue();
	EMIT("  ret\n");
}

static void codegen_kind_binary(koopa_raw_binary_t *binary)
{
	/* evaluate rhs first, since `m_reg_t_idx` works just like a stack */
	struct variant_t rhs = codegen_value(binary->rhs);
	struct variant_t lhs = codegen_value(binary->lhs);

	switch (binary->op)
	{
	case KOOPA_RBO_NOT_EQ:
		OPER("xor", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		OPER("snez", 2);
		break;
	case KOOPA_RBO_EQ:
		OPER("xor", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		OPER("seqz", 2);
		break;
	case KOOPA_RBO_GT:
		OPER("sgt", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_LT:
		OPER("slt", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_GE:
		OPER("slt", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		OPER("seqz", 2);
		break;
	case KOOPA_RBO_LE:
		OPER("sgt", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		OPER("seqz", 2);
		break;
	case KOOPA_RBO_ADD:
		OPER("add", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_SUB:
		OPER("sub", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_MUL:
		OPER("mul", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_DIV:
		OPER("div", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_MOD:
		OPER("rem", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_AND:
		OPER("and", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_OR:
		OPER("or", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_XOR:
		OPER("xor", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_SHL:
		OPER("sll", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_SHR:
		OPER("srl", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	case KOOPA_RBO_SAR:
		OPER("sra", 1);
		OPND(&lhs, false);
		OPND(&rhs, true);
		break;
	}
}

static void codegen_kind_integer(koopa_raw_integer_t *integer)
{
	if (integer->value != 0)
	{
		if (m_reg_t_idx < 7)
			EMIT("  li t%d, %d\n", m_reg_t_idx++, integer->value);
		else
			EMIT("  li a%d, %d\n", m_reg_t_idx++ - 7,
			     integer->value);
	}
}

/* visitors */
static void codegen_slice(const koopa_raw_slice_t *slice)
{
	switch (slice->kind)
	{
	case KOOPA_RSIK_UNKNOWN:
		assert(false /* unknown slice type */);
		break;
	case KOOPA_RSIK_TYPE:
		for (uint32_t i = 0; i < slice->len; ++i)
			codegen_type(slice->buffer[i]);
		break;
	case KOOPA_RSIK_FUNCTION:
		for (uint32_t i = 0; i < slice->len; ++i)
			codegen_function(slice->buffer[i]);
		break;
	case KOOPA_RSIK_BASIC_BLOCK:
		for (uint32_t i = 0; i < slice->len; ++i)
			codegen_basic_block(slice->buffer[i]);
		break;
	case KOOPA_RSIK_VALUE:
		for (uint32_t i = 0; i < slice->len; ++i)
		{
			koopa_raw_value_t value = slice->buffer[i];
			if (value->ty->tag == KOOPA_RTT_INT32)
				// register t_n should start from index of
				// current value that has non-unit type
				m_reg_t_idx = ++m_inst_idx;
			codegen_value(slice->buffer[i]);
		}
		break;
	}
}

static void codegen_slice_ref(const koopa_raw_slice_t *slice)
{
}

/* weird return type... i wonder if there's a better way to backtrack.
 * currently if we're not using `struct variant_t` we'll have to `ht_find()`
 * twice (first in this function, then in `OPND()`.) */
static struct variant_t codegen_value(koopa_raw_value_t raw)
{
	if (!raw)
		return (struct variant_t) { .tag = NIL, };

	if (raw->kind.tag == KOOPA_RVT_ALLOC)
	{
		uint32_t *offset_it = ht_find(m_ht_offsets, raw);
		assert(offset_it);
		return (struct variant_t) {
			.tag = OFFSET,
			.offset = *offset_it,
		};
	}

	uint32_t *value_it = ht_find(m_ht_insts, raw);
	if (value_it)
		return (struct variant_t) { .tag = INDEX, .index = *value_it, };

	codegen_type(raw->ty);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		codegen_kind_integer(&raw->kind.data.integer);
		break;
	case KOOPA_RVT_ZERO_INIT:
		assert(false /* todo */);
		break;
	case KOOPA_RVT_UNDEF:
		assert(false /* todo */);
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		assert(false /* todo */);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		assert(false /* todo */);
		break;
	case KOOPA_RVT_AGGREGATE:
		codegen_slice(&raw->kind.data.aggregate.elems);
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		codegen_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		codegen_kind_load(&raw->kind.data.load);
		ht_upsert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_STORE:
		codegen_kind_store(&raw->kind.data.store);
		ht_upsert(m_ht_insts, raw, m_inst_idx);
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
		codegen_kind_binary(&raw->kind.data.binary);
		ht_upsert(m_ht_insts, raw, m_inst_idx);
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
		codegen_kind_return(&raw->kind.data.ret);
		ht_upsert(m_ht_insts, raw, m_inst_idx);
		break;
	default:
		break;
	}

	return (struct variant_t) { .tag = VALUE, .value = raw };
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

	// TODO: really ugly, should be refactored
	function_prologue(raw);
	codegen_type(raw->ty);
	codegen_slice(&raw->params);
	codegen_slice(&raw->bbs);
}

static void codegen_type(koopa_raw_type_t raw)
{
	if (!raw)
		return;

	switch (raw->tag)
	{
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
void codegen(const koopa_raw_program_t *program, FILE *output)
{
	assert(output);

	m_output = output;
	m_ht_insts = ht_ptru32_new();
	m_ht_offsets = ht_ptru32_new();

#if 0
	koopa_raw_slice_t *values = &program->values;
	EMIT("  .data\n");
	codegen_slice(values);
#endif
	koopa_raw_slice_t *funcs = &program->funcs;
	EMIT("  .text\n");
	codegen_slice(funcs);

	ht_ptru32_delete(m_ht_offsets);
	ht_ptru32_delete(m_ht_insts);
}
