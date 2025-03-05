#undef _FORTIFY_SOURCE

#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hashtable.h"
#include "codegen.h"
#include "koopaext.h"
#include "macros.h"
#include "yield.h"

/* Optional<Variant<ValuePtr, ValueIndex, StackOffset>> */
struct variant_t {
	enum {
		NONE = 0,
		VALUE,
		INST,
		STACK,
	} tag;
	union {
		koopa_raw_value_t value;
		uint32_t inst;
		uint32_t stack;
	};
};

/* state variables */
static FILE *m_output;
static htable_ptru32_t m_ht_insts;
static htable_ptru32_t m_ht_stacks;

static uint32_t m_var_count;
static uint32_t m_val_count;
static int32_t m_stack_stack;
static int32_t m_inst_idx = -1;
static int32_t m_reg_t_idx;

/* emitters */
#define emit(format, ...) \
	fprintf(m_output, format __VA_OPT__(,) __VA_ARGS__)

/* register limits */
#define TMP_REGS 15
#define T_MAX 7
// `a6`, `a7` reserved for spilling
#define REG_MAX (TMP_REGS - 2)

/* getters */
#define m_inst_sp ((m_inst_idx - REG_MAX + m_var_count) * sizeof(uint32_t))
#define reg_t_sp ((reg_t_idx - REG_MAX + m_var_count) * sizeof(uint32_t))
#define variant_sp ((variant->inst - REG_MAX + m_var_count) * sizeof(uint32_t))

static void oper(const char *op, bool self_repeat)
{
	if (self_repeat)
	{
		if (m_inst_idx < T_MAX)
			emit("  %s t%d, t%d\n", op, m_inst_idx, m_inst_idx);
		else if (m_inst_idx < REG_MAX)
			emit("  %s a%d, a%d\n", op, m_inst_idx - T_MAX,
			     m_inst_idx - T_MAX);
		else
			emit("  %s a6, a6\n", op);
	}
	else
	{
		if (m_inst_idx < T_MAX)
			emit("  %s t%d, ", op, m_inst_idx);
		else if (m_inst_idx < REG_MAX)
			emit("  %s a%d, ", op, m_inst_idx - T_MAX);
		else
			emit("  %s a6, ", op);

		while (m_yield_end != 0)
		{
			next();
			if (m_yield_end != 0)
				emit(", ");
			else
				emit("\n");
		}
	}

	if (m_inst_idx >= REG_MAX)
		emit("  sw a6, %lu(sp)\n", m_inst_sp);
}

static void opnd(struct variant_t *variant)
{
	int32_t reg_t_idx = def(m_reg_t_idx);
	def(variant);

	switch (variant->tag)
	{
	case VALUE:
		++m_reg_t_idx;

		if (variant->value->kind.tag == KOOPA_RVT_INTEGER)
		{
			if (variant->value->kind.data.integer.value == 0)
				yield(emit("x0"));

			if (reg_t_idx < T_MAX)
				emit("  li t%d, %d\n", reg_t_idx,
				     variant->value->kind.data.integer.value);
			else if (reg_t_idx < REG_MAX)
				emit("  li a%d, %d\n", reg_t_idx - T_MAX,
				     variant->value->kind.data.integer.value);
			else
				emit("  li a%c, %d\n", '6' + m_yield_end,
				     variant->value->kind.data.integer.value);
		}

		if (reg_t_idx < T_MAX)
			yield(emit("t%d", use(m_reg_t_idx)));
		else if (reg_t_idx < REG_MAX)
			yield(emit("a%d", use(m_reg_t_idx) - T_MAX));

		if (variant->value->kind.tag != KOOPA_RVT_INTEGER)
			emit("  lw a%c, %lu(sp)\n", '6' + m_yield_end,
			     reg_t_sp);

		if (m_yield_end == 0)
			yield(emit("a6"));
		else if (m_yield_end == 1)
			yield(emit("a7"));
		break;
	case INST:
		if (variant->inst < T_MAX)
			yield(emit("t%d", use(variant)->inst));
		else if (variant->inst < REG_MAX)
			yield(emit("a%d", use(variant)->inst - T_MAX));

		emit("  lw a%c, %lu(sp)\n", '6' + m_yield_end, variant_sp);

		if (m_yield_end == 0)
			yield(emit("a6"));
		else if (m_yield_end == 1)
			yield(emit("a7"));
		break;
	case STACK:
		yield(emit("%u(sp)", use(variant)->stack));
		break;
	default:
		panic("value is NONE");
	}
}

static void inst(const char *op, ...)
{
	va_list args;
	va_start(args, op);

	struct variant_t *arg;
	while (true)
	{
		arg = va_arg(args, struct variant_t *);
		if (!arg)
			break;

		opnd(arg);
	}

	oper(op, false);
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
		if (value->ty->tag == KOOPA_RTT_POINTER)
		{
			htable_ptru32_insert(m_ht_stacks, value,
					     -m_stack_stack);
			++m_var_count;
			m_stack_stack -= sizeof(int32_t);
		}
		else if (value->ty->tag == KOOPA_RTT_INT32)
			if (++m_val_count + m_var_count > REG_MAX)
				m_stack_stack -= sizeof(int32_t);
	}
}

/* allocator */
static void function_prologue(koopa_raw_function_t function)
{
	slice_iter(&function->bbs, reserve_stack);
	// align to 16 bytes
	m_stack_stack &= -16;
	emit("  addi sp, sp, %d\n", m_stack_stack);
}

/* kind generators */
static void raw_kind_load(koopa_raw_load_t *load)
{
	struct variant_t src = raw_value(load->src);

	inst("lw", &src, NULL);
}

static void raw_kind_branch(koopa_raw_branch_t *branch)
{
	struct variant_t cond = raw_value(branch->cond);

	opnd(&cond);
	emit("  bnez ");
	next();
	emit(", %s\n", branch->true_bb->name + 1);
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

	opnd(&value);
	opnd(&dest);
	emit("  sw ");
	next();
	emit(", ");
	next();
	emit("\n");
}

static void raw_kind_return(koopa_raw_return_t *ret)
{
	struct variant_t value = raw_value(ret->value);

	opnd(&value);
	emit("  mv a0, ");
	next();
	emit("\n");
	emit("  addi sp, sp, %d\n", -m_stack_stack);
	emit("  ret\n");
}

static void raw_kind_binary(koopa_raw_binary_t *binary)
{
	struct variant_t lhs = raw_value(binary->lhs);
	struct variant_t rhs = raw_value(binary->rhs);

	switch (binary->op)
	{
	case KOOPA_RBO_NOT_EQ:
		inst("xor", &lhs, &rhs, NULL);
		oper("snez", 2);
		break;
	case KOOPA_RBO_EQ:
		inst("xor", &lhs, &rhs, NULL);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_GT:
		inst("sgt", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_LT:
		inst("slt", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_GE:
		inst("slt", &lhs, &rhs, NULL);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_LE:
		inst("sgt", &lhs, &rhs, NULL);
		oper("seqz", 2);
		break;
	case KOOPA_RBO_ADD:
		inst("add", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_SUB:
		inst("sub", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_MUL:
		inst("mul", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_DIV:
		inst("div", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_MOD:
		inst("rem", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_AND:
		inst("and", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_OR:
		inst("or", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_XOR:
		inst("xor", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_SHL:
		inst("sll", &lhs, &rhs, NULL);
		break;
	case KOOPA_RBO_SHR:
		unimplemented();
		break;
	case KOOPA_RBO_SAR:
		inst("sra", &lhs, &rhs, NULL);
		break;
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
			if (value->ty->tag != KOOPA_RTT_UNIT)
				// register t_n should start from index of
				// current value that has non-unit type.
				// TODO optimize stepping strategy
				m_reg_t_idx = ++m_inst_idx;
			raw_value(slice->buffer[i]);
		}
		break;
	}
}

/* weird return type... i wonder if there's a better way to backtrack. currently
 * if we're not using `struct variant_t` we'll have to call `htable_lookup()`
 * twice: first in this function, then in `opnd()`. */
static struct variant_t raw_value(koopa_raw_value_t raw)
{
	if (!raw)
		return (struct variant_t) { .tag = NONE, };

	if (raw->kind.tag == KOOPA_RVT_ALLOC)
	{
		uint32_t *stack_it = htable_lookup(m_ht_stacks, raw);
		assert(stack_it);

		return (struct variant_t) {
			.tag = STACK,
			.stack = *stack_it,
		};
	}

	uint32_t *value_it = htable_lookup(m_ht_insts, raw);
	if (value_it)
		return (struct variant_t) { .tag = INST, .inst = *value_it, };

	raw_type(raw->ty);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		/* do nothing */
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
	// raw_slice(&raw->used_by);
	raw_slice(&raw->insts);
	
	m_inst_idx = -1;
}

static void raw_function(koopa_raw_function_t raw)
{
	if (!raw)
		return;

	// every function is global (`extern`al)
	emit("  .globl %s\n", raw->name + 1);
	emit("%s:\n", raw->name + 1);

	// TODO really ugly, should be refactored
	function_prologue(raw);

	raw_type(raw->ty);
	raw_slice(&raw->params);
	raw_slice(&raw->bbs);

	m_stack_stack = 0;
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
	m_ht_stacks = htable_ptru32_new();

#if 0
	koopa_raw_slice_t *values = &program->values;
	emit("  .data\n");
	raw_slice(values);
#endif
	koopa_raw_slice_t *funcs = &program->funcs;
	emit("  .text\n");
	raw_slice(funcs);

	htable_ptru32_delete(m_ht_stacks);
	htable_ptru32_delete(m_ht_insts);
}
