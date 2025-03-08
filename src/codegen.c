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
#include "vector.h"
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
static uint32_t m_arg_spill;
static int32_t m_stack_offset;
/* TODO add checks. reg_t_idx should not be smaller than inst_idx */
static int32_t m_reg_t_idx;
static int32_t m_inst_idx = -1;

/* emitters */
#define emit(format, ...) \
	fprintf(m_output, format __VA_OPT__(,) __VA_ARGS__)

/* register limits */
#define T_MAX 7
#define A_MAX 8
// a6, a7 reserved for spilling
#define R_MAX (T_MAX + A_MAX - 2)

/* getters */
#define saved_regs min(m_val_count, R_MAX)

#define m_inst_sp \
	((m_inst_idx - R_MAX + m_var_count + saved_regs + m_arg_spill) \
	 * sizeof(int32_t))
#define reg_t_sp \
	((reg_t_idx - R_MAX + m_var_count + saved_regs + m_arg_spill) \
	 * sizeof(int32_t))
#define variant_sp \
	((variant->inst - R_MAX + m_var_count + saved_regs + m_arg_spill) \
	 * sizeof(int32_t))

static void oper(const char *op, bool self_repeat)
{
	if (self_repeat)
	{
		if (m_inst_idx < T_MAX)
			emit("  %s t%d, t%d\n", op, m_inst_idx, m_inst_idx);
		else if (m_inst_idx < R_MAX)
			emit("  %s a%d, a%d\n", op, m_inst_idx - T_MAX,
			     m_inst_idx - T_MAX);
		else
			emit("  %s a6, a6\n", op);
	}
	else
	{
		if (m_inst_idx < T_MAX)
			emit("  %s t%d, ", op, m_inst_idx);
		else if (m_inst_idx < R_MAX)
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

	if (m_inst_idx >= R_MAX)
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
			else if (reg_t_idx < R_MAX)
				emit("  li a%d, %d\n", reg_t_idx - T_MAX,
				     variant->value->kind.data.integer.value);
			else
				emit("  li a%c, %d\n", '6' + m_yield_end,
				     variant->value->kind.data.integer.value);
		}

		if (reg_t_idx < T_MAX)
			yield(emit("t%d", use(m_reg_t_idx)));
		else if (reg_t_idx < R_MAX)
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
		else if (variant->inst < R_MAX)
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

/* register allocation */
static void count_vars(void *basic_block)
{
	koopa_raw_basic_block_t _basic_block = basic_block;

	for (uint32_t i = 0; i < _basic_block->insts.len; ++i)
	{
		koopa_raw_value_t value = _basic_block->insts.buffer[i];
		if (value->ty->tag == KOOPA_RTT_POINTER)
			htable_insert(m_ht_stacks, value,
				      m_var_count++ * sizeof(int32_t));
	}
}

/* really need some kind of garbage collection */
static struct vector_u32_t *m_val_counts;
static void count_vals(void *basic_block)
{
	koopa_raw_basic_block_t _basic_block = basic_block;

	uint32_t val_count = 0;
	for (uint32_t i = 0; i < _basic_block->insts.len; ++i)
	{
		koopa_raw_value_t value = _basic_block->insts.buffer[i];

		if (value->ty->tag == KOOPA_RTT_INT32)
			++val_count;
	}
	vector_u32_push(m_val_counts, val_count);
}

static struct vector_u32_t *m_arg_counts;
static void count_args(void *basic_block)
{
	koopa_raw_basic_block_t _basic_block = basic_block;

	uint32_t arg_count = 0;
	for (uint32_t i = 0; i < _basic_block->insts.len; ++i)
	{
		koopa_raw_value_t value = _basic_block->insts.buffer[i];

		if (value->kind.tag == KOOPA_RVT_CALL)
			arg_count += value->kind.data.call.args.len;
	}
	vector_u32_push(m_arg_counts, arg_count);
}

#if 0
typedef uint32_t (*acc_u32_t)(uint32_t a, uint32_t acc);

static uint32_t max_u32(uint32_t a, uint32_t acc)
{
	return max(a, acc);
}

static uint32_t slice_fold(koopa_raw_slice_t *slice, acc_u32_t acc,
			   uint32_t init)
{
}
#endif

static void function_prologue(koopa_raw_function_t function)
{
	slice_iter(&function->bbs, count_vars);

	/* let max_val_count =
	 *   List.map function_bbs count_vals |> List.fold_left max 0;; */
	uint32_t max_val_count = 0;
	m_val_counts = vector_u32_new(function->bbs.len);
	slice_iter(&function->bbs, count_vals);
	for (size_t i = 0; i < m_val_counts->size; ++i)
		max_val_count = max(m_val_counts->data[i], max_val_count);
	vector_u32_delete(m_val_counts);
	m_val_count = max_val_count;

	/* let max_arg_count =
	 *   List.map function_bbs count_args |> List.fold_left max 0;; */
	uint32_t max_arg_count = 0;
	m_arg_counts = vector_u32_new(function->bbs.len);
	slice_iter(&function->bbs, count_args);
	for (size_t i = 0; i < m_arg_counts->size; ++i)
		max_arg_count = max(m_arg_counts->data[i], max_arg_count);
	vector_u32_delete(m_arg_counts);
	m_arg_spill = max(max_arg_count, A_MAX) - A_MAX;

	/* (high)
	 * 1. return address;
	 * 2. spilled values;
	 * 3. local variables;
	 * 4. saved registers;
	 * 5. spilled arguments;
	 * (low) */
	uint32_t total = 0;
	total += sizeof(uint32_t);
	total += m_var_count;
	total += max(max_val_count, R_MAX) - R_MAX;
	total += min(max_val_count, R_MAX);
	total += m_arg_spill;

	// align to 16 bytes
	m_stack_offset = -(total * sizeof(int32_t)) & -16;
	emit("  addi sp, sp, %d\n", m_stack_offset);
	emit("  sw ra, %lu(sp)\n", -m_stack_offset - sizeof(uint32_t));

	printf("%s(%u): val = %u, var = %u, stack = %d\n", function->name,
	       function->params.len, max_val_count, m_var_count,
	       m_stack_offset);
}

static void function_epilogue()
{
	m_var_count = 0;
	m_val_count = 0;
	m_arg_spill = 0;
	m_stack_offset = 0;
	m_reg_t_idx = 0;
	m_inst_idx = 0;
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
	if (ret->value)
	{
		struct variant_t value = raw_value(ret->value);

		opnd(&value);
		emit("  mv a0, ");
		next();
		emit("\n");
	}
	emit("  lw ra, %lu(sp)\n", -m_stack_offset - sizeof(uint32_t));
	emit("  addi sp, sp, %d\n", -m_stack_offset);
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
			if (value->ty->tag == KOOPA_RTT_INT32)
				// register t_n should start from index of
				// current value that has non-unit type.
				// TODO optimize stepping strategy
				m_reg_t_idx = ++m_inst_idx;
			raw_value(slice->buffer[i]);
		}
		break;
	}
}

static void raw_kind_call(const koopa_raw_call_t *call)
{
	const koopa_raw_function_t callee = call->callee;
	const koopa_raw_slice_t *args = &call->args;

	/* parameters */
	struct variant_t arg;
	for (uint32_t i = 0; i < min(args->len, A_MAX); ++i)
	{
		arg = raw_value(args->buffer[i]);

		emit("  mv a%c, ", '0' + i);
		opnd(&arg);
		next();
		emit("\n");
	}
	if (args->len > A_MAX)
		for (uint32_t i = A_MAX; i < args->len; ++i)
		{
			uint32_t offset = (i - A_MAX) * sizeof(int32_t);
			arg = raw_value(args->buffer[i]);

			emit("  sw ");
			opnd(&arg);
			next();
			emit(", %u(sp)\n", offset);
		}

	/* saved registers */
	for (uint32_t i = 0; i < min(m_val_count, R_MAX); ++i)
		if (i < T_MAX)
			emit("  sw t%d, %lu(sp)\n", i,
			     (i + m_arg_spill) * sizeof(int32_t));
		else
			emit("  sw a%d, %lu(sp)\n", i - T_MAX,
			     (i + m_arg_spill) * sizeof(int32_t));

	emit("  call %s\n", callee->name + 1);

	// TODO refactor: synchronicity
	if (m_inst_idx < T_MAX)
		emit("  mv t%d, a0\n", m_inst_idx);
	else if (m_inst_idx < R_MAX)
		emit("  mv a%d, a0\n", m_inst_idx);
	else
		emit("  sw a0, %lu(sp)\n", m_inst_sp);

	/* pop caller-saved */
	for (uint32_t i = 0; i < min(m_val_count, R_MAX); ++i)
		if (i < T_MAX)
			emit("  lw t%d, %lu(sp)\n", i,
			     (i + m_arg_spill) * sizeof(int32_t));
		else
			emit("  lw a%d, %lu(sp)\n", i - T_MAX,
			     (i + m_arg_spill) * sizeof(int32_t));
}

static void raw_kind_func_arg_ref(const koopa_raw_func_arg_ref_t *func_arg_ref)
{
	(void) func_arg_ref;
}

/* weird return type... i wonder if there's a better way to backtrack. currently
 * if we're not using `struct variant_t` we'll have to call `htable_lookup()`
 * twice: first in this function, then in `opnd()`.
 * TODO this devastatingly needs to be refactored */
static struct variant_t raw_value(koopa_raw_value_t raw)
{
	if (!raw)
		return (struct variant_t) { .tag = NONE, };

	/* KOOPA_RTT_POINTER */
	uint32_t *stack_it = htable_lookup(m_ht_stacks, raw);
	if (stack_it)
		return (struct variant_t) { .tag = STACK, .stack = *stack_it, };

	/* KOOPA_RTT_INT32 */
	uint32_t *inst_it = htable_lookup(m_ht_insts, raw);
	if (inst_it)
		return (struct variant_t) { .tag = INST, .inst = *inst_it, };

	raw_type(raw->ty);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		/* immediate */
		break;
	case KOOPA_RVT_ZERO_INIT:
		todo();
		break;
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		raw_kind_func_arg_ref(&raw->kind.data.load);
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		unimplemented();
		break;
	case KOOPA_RVT_ALLOC:
		/* stack */
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
		break;
	case KOOPA_RVT_JUMP:
		raw_kind_jump(&raw->kind.data.jump);
		break;
	case KOOPA_RVT_CALL:
		raw_kind_call(&raw->kind.data.call);
		htable_insert(m_ht_insts, raw, m_inst_idx);
		break;
	case KOOPA_RVT_RETURN:
		raw_kind_return(&raw->kind.data.ret);
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
	if (strcmp(raw->name, "entry") == 0)
		emit("  sw pc, 0(sp)\n");
	// raw_slice(&raw->params);
	// raw_slice(&raw->used_by);
	raw_slice(&raw->insts);
	
	m_inst_idx = -1;
}

static void raw_function(koopa_raw_function_t raw)
{
	if (!raw)
		return;

	/* declaration only */
	if (raw->bbs.len == 0)
		return;

	// every function is global (`extern`al)
	emit("\n  .globl %s\n", raw->name + 1);
	emit("%s:\n", raw->name + 1);

	// TODO really ugly, should be refactored
	function_prologue(raw);

	raw_type(raw->ty);
	raw_slice(&raw->params);
	raw_slice(&raw->bbs);

	function_epilogue();
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
