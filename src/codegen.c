#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hashtable.h"
#include "codegen.h"
#include "koopaext.h"
#include "macros.h"

/* Optional<Variant<ValuePtr, ValueIndex, StackOffset>> */
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

static htable_ptru32_t m_ht_insts;
static htable_ptru32_t m_ht_offsets;

static uint32_t m_inst_idx;
static uint8_t m_reg_t_idx;
static int m_stack_offset;

/* emitter */
#define emit(format, ...) fprintf(m_output, format __VA_OPT__(,) __VA_ARGS__)

static void oper(const char *op, unsigned times)
{
	assert(times >= 0 && times <= 3);

	switch (times)
	{
	case 0:
		/* basically dedicated for `sw` */
		emit("  %s ", op);
		break;
	case 1:
		/* one output */
		if (m_inst_idx < 7)
			emit("  %s t%d, ", op, m_inst_idx);
		else
			emit("  %s a%d, ", op, m_inst_idx - 7);
		break;
	case 2:
		/* self repeating */
		if (m_inst_idx < 7)
			emit("  %s t%d, t%d\n", op, m_inst_idx, m_inst_idx);
		else
			emit("  %s a%d, a%d\n", op, m_inst_idx - 7,
			     m_inst_idx - 7);
		break;
	case 3:
		if (m_inst_idx < 7)
			emit("  %s t%d, t%d, t%d\n", op, m_inst_idx, m_inst_idx,
			     m_inst_idx);
		else
			emit("  %s a%d, a%d, a%d\n", op, m_inst_idx - 7,
			     m_inst_idx - 7, m_inst_idx - 7);
		break;
	}
}

static void opnd(struct variant_t *variant, bool newline)
{
	switch (variant->tag)
	{
	case VALUE:
		if (variant->value->kind.tag == KOOPA_RVT_INTEGER &&
		    variant->value->kind.data.integer.value == 0)
			emit(newline ? "x0\n" : "x0, ");
		else
		{
			if (--m_reg_t_idx < 7)
				emit(newline ? "t%d\n" : "t%d, ", m_reg_t_idx);
			else
				emit(newline ? "a%d\n" : "a%d, ",
				     m_reg_t_idx - 7);
		}
		break;
	case INDEX:
		if (variant->index < 7)
			emit(newline ? "t%d\n" : "t%d, ", variant->index);
		else
			emit(newline ? "a%d\n" : "a%d, ", variant->index - 7);
		break;
	case OFFSET:
		emit(newline ? "%d(sp)\n" : "%d(sp)", variant->offset);
		break;
	default:
		panic("missing value");
	}
}

/* declarations */
static struct variant_t raw_value(koopa_raw_value_t raw);
static void raw_basic_block(koopa_raw_basic_block_t basic_block);
static void raw_function(koopa_raw_function_t function);
static void raw_type(koopa_raw_type_t type);

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
		htable_ptru32_insert(m_ht_offsets, value, -m_stack_offset);
		m_stack_offset -= sizeof(int32_t);
	}
}

/* allocator */
static void function_prologue(koopa_raw_function_t function)
{
	slice_iter(&function->bbs, reserve_stack);
	// align to 16 bytes
	m_stack_offset &= -16;
	emit("  addi sp, sp, %d\n", m_stack_offset);
}

static void function_epilogue(void)
{
	emit("  addi sp, sp, %d\n", -m_stack_offset);
	// reset stack offset
	m_stack_offset = 0;
}

/* kind generators */
static void raw_kind_load(koopa_raw_load_t *load)
{
	struct variant_t src = raw_value(load->src);

	oper("lw", 1);
	opnd(&src, true);
}

static void raw_kind_branch(koopa_raw_branch_t *branch)
{
	struct variant_t cond = raw_value(branch->cond);

	oper("bnez", 0);
	opnd(&cond, false);
	emit("%s\n", branch->true_bb->name + 1);
	emit("  j %s\n", branch->false_bb->name + 1);
}

static void raw_kind_jump(koopa_raw_jump_t *jump)
{
	emit("  j %s\n", jump->target->name + 1);
}

static void raw_kind_store(koopa_raw_store_t *store)
{
	struct variant_t value = raw_value(store->value);
	struct variant_t dest = raw_value(store->dest);

	oper("sw", 0);
	opnd(&value, false);
	opnd(&dest, true);
}

static void raw_kind_return(koopa_raw_return_t *ret)
{
	struct variant_t value = raw_value(ret->value);

	emit("  mv a0, ");
	opnd(&value, true);
	function_epilogue();
	emit("  ret\n");
}

static void raw_kind_binary(koopa_raw_binary_t *binary)
{
	/* evaluate rhs first, since `m_reg_t_idx` works just like a stack */
	struct variant_t rhs = raw_value(binary->rhs);
	struct variant_t lhs = raw_value(binary->lhs);

	switch (binary->op)
	{
	case KOOPA_RBO_NOT_EQ:
		oper("xor", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		oper("snez", 2);
		break;
	case KOOPA_RBO_EQ:
		oper("xor", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_GT:
		oper("sgt", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_LT:
		oper("slt", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_GE:
		oper("slt", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_LE:
		oper("sgt", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_ADD:
		oper("add", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_SUB:
		oper("sub", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_MUL:
		oper("mul", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_DIV:
		oper("div", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_MOD:
		oper("rem", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_AND:
		oper("and", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_OR:
		oper("or", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_XOR:
		oper("xor", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_SHL:
		oper("sll", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	case KOOPA_RBO_SHR:
		unimplemented();
		break;
	case KOOPA_RBO_SAR:
		oper("sra", 1);
		opnd(&lhs, false);
		opnd(&rhs, true);
		break;
	}
}

static void raw_kind_integer(koopa_raw_integer_t *integer)
{
	if (integer->value != 0)
	{
		if (m_reg_t_idx < 7)
			emit("  li t%d, %d\n", m_reg_t_idx++, integer->value);
		else
			emit("  li a%d, %d\n", m_reg_t_idx++ - 7,
			     integer->value);
	}
}

/* visitors */
static void raw_slice(const koopa_raw_slice_t *slice)
{
	switch (slice->kind)
	{
	case KOOPA_RSIK_UNKNOWN:
		panic("unknown slice type");
		break;
	case KOOPA_RSIK_TYPE:
		for (uint32_t i = 0; i < slice->len; ++i)
			raw_type(slice->buffer[i]);
		break;
	case KOOPA_RSIK_FUNCTION:
		for (uint32_t i = 0; i < slice->len; ++i)
			raw_function(slice->buffer[i]);
		break;
	case KOOPA_RSIK_BASIC_BLOCK:
		for (uint32_t i = 0; i < slice->len; ++i)
			raw_basic_block(slice->buffer[i]);
		break;
	case KOOPA_RSIK_VALUE:
		for (uint32_t i = 0; i < slice->len; ++i)
		{
			koopa_raw_value_t value = slice->buffer[i];
			if (value->ty->tag == KOOPA_RTT_INT32)
				// register t_n should start from index of
				// current value that has non-unit type
				m_reg_t_idx = ++m_inst_idx;
			raw_value(slice->buffer[i]);
		}
		break;
	}
}

/* weird return type... i wonder if there's a better way to backtrack.
 * currently if we're not using `struct variant_t` we'll have to call
 * `htable_lookup()` twice: first in this function, then in `opnd()`. */
static struct variant_t raw_value(koopa_raw_value_t raw)
{
	if (!raw)
		return (struct variant_t) { .tag = NIL, };

	if (raw->kind.tag == KOOPA_RVT_ALLOC)
	{
		uint32_t *offset_it = htable_lookup(m_ht_offsets, raw);
		assert(offset_it);
		return (struct variant_t) {
			.tag = OFFSET,
			.offset = *offset_it,
		};
	}

	uint32_t *value_it = htable_lookup(m_ht_insts, raw);
	if (value_it)
		return (struct variant_t) { .tag = INDEX, .index = *value_it, };

	raw_type(raw->ty);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		raw_kind_integer(&raw->kind.data.integer);
		break;
	case KOOPA_RVT_ZERO_INIT:
		todo();
		break;
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		todo();
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		unimplemented();
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		todo();
		raw_value(raw->kind.data.global_alloc.init);
		break;
	case KOOPA_RVT_LOAD:
		raw_kind_load(&raw->kind.data.load);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_STORE:
		raw_kind_store(&raw->kind.data.store);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_GET_PTR:
		todo();
		raw_value(raw->kind.data.get_ptr.src);
		raw_value(raw->kind.data.get_ptr.index);
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		todo();
		raw_value(raw->kind.data.get_elem_ptr.src);
		raw_value(raw->kind.data.get_elem_ptr.index);
		break;
	case KOOPA_RVT_BINARY:
		raw_kind_binary(&raw->kind.data.binary);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_BRANCH:
		raw_kind_branch(&raw->kind.data.branch);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_JUMP:
		raw_kind_jump(&raw->kind.data.jump);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_CALL:
		todo();
		raw_function(raw->kind.data.call.callee);
		raw_slice(&raw->kind.data.call.args);
		break;
	case KOOPA_RVT_RETURN:
		raw_kind_return(&raw->kind.data.ret);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	default:
		break;
	}

	return (struct variant_t) { .tag = VALUE, .value = raw };
}

static void raw_basic_block(koopa_raw_basic_block_t raw)
{
	if (!raw)
		return;

	emit("%s:\n", raw->name + 1);
	raw_slice(&raw->params);
#if 0
	raw_slice(&raw->used_by);
#endif
	raw_slice(&raw->insts);
}

static void raw_function(koopa_raw_function_t raw)
{
	if (!raw)
		return;

	if (strcmp(raw->name, "@main") == 0)
		emit("  .globl main\n");
	emit("%s:\n", raw->name + 1);

	// TODO: really ugly, should be refactored
	function_prologue(raw);
	raw_type(raw->ty);
	raw_slice(&raw->params);
	raw_slice(&raw->bbs);
}

static void raw_type(koopa_raw_type_t raw)
{
	if (!raw)
		return;

	switch (raw->tag)
	{
	case KOOPA_RTT_INT32:
	case KOOPA_RTT_UNIT:
		break;
	case KOOPA_RTT_ARRAY:
		raw_type(raw->data.array.base);
		break;
	case KOOPA_RTT_POINTER:
		raw_type(raw->data.pointer.base);
		break;
	case KOOPA_RTT_FUNCTION:
		raw_slice(&raw->data.function.params);
		raw_type(raw->data.function.ret);
		break;	
	}
}

/* public defn.s */
void codegen(const koopa_raw_program_t *program, FILE *output)
{
	assert(output);

	m_output = output;
	m_ht_insts = htable_ptru32_new();
	m_ht_offsets = htable_ptru32_new();

#if 0
	koopa_raw_slice_t *values = &program->values;
	emit("  .data\n");
	raw_slice(values);
#endif
	koopa_raw_slice_t *funcs = &program->funcs;
	emit("  .text\n");
	raw_slice(funcs);

	htable_ptru32_delete(m_ht_offsets);
	htable_ptru32_delete(m_ht_insts);
}
