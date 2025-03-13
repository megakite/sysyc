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

/* Optional<Variant<ValuePtr, ValueIndex, StackOffset, GlobalAddress>> */
struct variant_t {
	enum {
		NONE = 0,
		VALUE,
		OUT,
		STACK,
		GLOBAL,
	} tag;
	union {
		koopa_raw_value_t value;
		uint32_t out;
		uint32_t stack;
		koopa_raw_value_t global;
	};
};

/* state variables */
static FILE *m_output;
static htable_ppuu32_t m_ht_outs;
static htable_ptru32_t m_ht_stacks;

/* per-function context */
static struct {
	uint32_t var_count;
	uint32_t val_count;
	uint32_t arg_count;
	uint32_t par_spill;
	uint32_t stack_size;
	uint32_t bb_idx;
} m_fn;

/* per-basic block context */
static struct {
	uint32_t reg_idx;
	uint32_t out_idx;
} m_bb;

/* emitters */
#define emit(format, ...) \
	fprintf(m_output, format __VA_OPT__(,) __VA_ARGS__)

/* register limits */
// t5, t6 reserved for spilling
#define T_MAX (7 - 2)
#define A_MAX 8
#define R_MAX (T_MAX + A_MAX)

/* getters */
#define saved_regs min(m_fn.val_count, R_MAX)
#define spill_vals max(m_fn.val_count, R_MAX) - R_MAX
#define regst_args min(m_fn.arg_count, A_MAX)
#define spill_args max(m_fn.arg_count, A_MAX) - A_MAX

#define m_out_sp \
	((m_bb.out_idx - R_MAX + m_fn.var_count + saved_regs + spill_args) \
	 * sizeof(int32_t))
#define reg_sp \
	((reg_idx - R_MAX + m_fn.var_count + saved_regs + spill_args) \
	 * sizeof(int32_t))
#define variant_sp \
	((variant->out - R_MAX + m_fn.var_count + saved_regs + spill_args) \
	 * sizeof(int32_t))

static void oper(const char *op, bool self_repeat)
{
	if (self_repeat)
	{
		if (m_bb.out_idx < T_MAX)
			emit("  %s t%d, t%d\n", op, m_bb.out_idx, m_bb.out_idx);
		else if (m_bb.out_idx < R_MAX - regst_args)
			emit("  %s a%d, a%d\n", op,
			     m_bb.out_idx - T_MAX + m_fn.arg_count,
			     m_bb.out_idx - T_MAX + m_fn.arg_count);
		else
			emit("  %s t5, t5\n", op);
	}
	else
	{
		if (m_bb.out_idx < T_MAX)
			emit("  %s t%d, ", op, m_bb.out_idx);
		else if (m_bb.out_idx < R_MAX - regst_args)
			emit("  %s a%d, ", op,
			     m_bb.out_idx - T_MAX + m_fn.arg_count);
		else
			emit("  %s t5, ", op);

		while (m_yield_end != 0)
		{
			next();
			if (m_yield_end != 0)
				emit(", ");
			else
				emit("\n");
		}
	}

	if (m_bb.out_idx >= R_MAX - regst_args)
		emit("  sw t5, %lu(sp)\n", m_out_sp);
}

static void opnd(struct variant_t *variant)
{
	uint32_t reg_idx = def(m_bb.reg_idx);
	def(variant);

	switch (variant->tag)
	{
	case VALUE:
		/* immediate */
		if (variant->value->kind.tag == KOOPA_RVT_INTEGER)
		{
			uint32_t value =
				variant->value->kind.data.integer.value;

			if (value == 0)
				yield(emit("x0"));

			if (reg_idx < T_MAX)
				emit("  li t%d, %d\n", reg_idx, value);
			else if (reg_idx < R_MAX - regst_args)
				emit("  li a%d, %d\n",
				     reg_idx - T_MAX + m_fn.arg_count, value);
			else
				emit("  li t%c, %d\n", '5' + m_yield_end,
				     value);
		}

		/* register */
		++m_bb.reg_idx;

		if (reg_idx < T_MAX)
			yield(emit("t%d", use(m_bb.reg_idx)));
		else if (reg_idx < R_MAX - regst_args)
			yield(emit("a%d",
				   use(m_bb.reg_idx) - T_MAX + m_fn.arg_count));

		/* stack */
		if (variant->value->kind.tag != KOOPA_RVT_INTEGER)
			emit("  lw t%c, %lu(sp)\n", '5' + m_yield_end, reg_sp);

		if (m_yield_end == 0)
			yield(emit("t5"));
		else if (m_yield_end == 1)
			yield(emit("t6"));
		break;
	case OUT:
		if (variant->out < T_MAX)
			yield(emit("t%d", use(variant)->out));
		else if (variant->out < R_MAX - regst_args)
			yield(emit("a%d",
				   use(variant)->out - T_MAX + m_fn.arg_count));

		emit("  lw t%c, %lu(sp)\n", '5' + m_yield_end, variant_sp);

		if (m_yield_end == 0)
			yield(emit("t5"));
		else if (m_yield_end == 1)
			yield(emit("t6"));
		break;
	case STACK:
		yield(emit("%u(sp)", use(variant)->stack));
		break;
	case GLOBAL:
		++m_bb.reg_idx;

		if (reg_idx < T_MAX)
		{
			emit("  la t%d, %s\n", reg_idx,
			     variant->global->name + 1);
			yield(emit("0(t%d)", use(m_bb.reg_idx)));
		}
		else if (reg_idx < R_MAX - regst_args)
		{
			emit("  la a%d, %s\n", reg_idx - T_MAX + m_fn.arg_count,
			     variant->global->name + 1);
			yield(emit("0(a%d)",
				   use(m_bb.reg_idx) - T_MAX + m_fn.arg_count));
		}

		emit("  la t%c, %s\n", '5' + m_yield_end,
		     variant->global->name + 1);

		if (m_yield_end == 0)
			yield(emit("0(t5)"));
		else if (m_yield_end == 1)
			yield(emit("0(t6)"));
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
static void count_vars(koopa_raw_function_t function)
{
	for (uint32_t i = 0; i < function->bbs.len; ++i)
	{
		koopa_raw_basic_block_t basic_block = function->bbs.buffer[i];

		for (uint32_t j = 0; j < basic_block->insts.len; ++j)
		{
			koopa_raw_value_t value = basic_block->insts.buffer[j];

			if (value->kind.tag != KOOPA_RVT_ALLOC)
				continue;

			/* process spilled params later */
			if (m_fn.var_count + m_fn.par_spill
			    < function->params.len
			    && m_fn.var_count + m_fn.par_spill >= A_MAX)
			{
				++m_fn.par_spill;
				continue;
			}

			uint32_t offset = sizeof(int32_t)
					  * (saved_regs + spill_args
					     + m_fn.var_count);
			htable_insert(m_ht_stacks, value, offset);
			++m_fn.var_count;
		}
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

static void function_prologue(koopa_raw_function_t function)
{
	/* val_count <- List.map function_bbs count_vals
	 *              |> List.fold_left max 0;; */
	m_val_counts = vector_u32_new(function->bbs.len);
	slice_iter(&function->bbs, count_vals);
	for (size_t i = 0; i < m_val_counts->size; ++i)
		m_fn.val_count = max(m_val_counts->data[i], m_fn.val_count);
	vector_u32_delete(m_val_counts);

	/* arg_count <- List.map function_bbs count_args
	 *              |> List.fold_left max 0;; */
	m_arg_counts = vector_u32_new(function->bbs.len);
	slice_iter(&function->bbs, count_args);
	for (size_t i = 0; i < m_arg_counts->size; ++i)
		m_fn.arg_count = max(m_arg_counts->data[i], m_fn.arg_count);
	vector_u32_delete(m_arg_counts);

	count_vars(function);

	/* (high)
	 * 1. return address;
	 * 2. spilled values;
	 * 3. local variables;
	 * 4. saved registers;
	 * 5. spilled arguments;
	 * (low) */
	uint32_t total = 0;
	// FIXME weird behavior reserving return address
	total += 4;
	total += spill_vals;
	total += m_fn.var_count;
	total += saved_regs;
	total += spill_args;

	// align to 16 bytes
	m_fn.stack_size = -(-(total * sizeof(int32_t)) & -16);
	emit("  addi sp, sp, %d\n", -m_fn.stack_size);
	emit("  sw ra, %lu(sp)\n", m_fn.stack_size - sizeof(uint32_t));

	printf("%s(%u): val = %u, var = %u, par = %u, stack = %d\n",
	       function->name, function->params.len, m_fn.val_count,
	       m_fn.var_count, m_fn.par_spill, m_fn.stack_size);
}

static void function_epilogue()
{
	memset(&m_fn, 0, sizeof(m_fn));
	memset(&m_bb, 0, sizeof(m_bb));
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
	if (store->value->kind.tag == KOOPA_RVT_FUNC_ARG_REF)
	{
		uint32_t index = store->value->kind.data.func_arg_ref.index;

		if (index < A_MAX)
			emit("  sw a%u, %lu(sp)\n", index,
			     sizeof(uint32_t) * (spill_args + saved_regs
						 + index));
		else
			htable_insert(m_ht_stacks, store->dest,
				      sizeof(uint32_t) * (index - A_MAX)
				      + m_fn.stack_size);

		return;
	}

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
	emit("  lw ra, %lu(sp)\n", m_fn.stack_size - sizeof(uint32_t));
	emit("  addi sp, sp, %d\n", m_fn.stack_size);
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

			raw_value(value);
		       	if (value->ty->tag == KOOPA_RTT_INT32)
				// register t_n should start from index of
				// current value that has non-unit type.
				// TODO optimize stepping strategy
				m_bb.reg_idx = ++m_bb.out_idx;
		}
		break;
	}
}

static void raw_kind_call(const koopa_raw_call_t *call)
{
	const koopa_raw_function_t callee = call->callee;
	const koopa_raw_slice_t *args = &call->args;

	/* push saved registers */
	for (uint32_t i = 0; i < min(m_bb.out_idx, R_MAX); ++i)
		if (i < T_MAX)
			emit("  sw t%d, %lu(sp)\n", i,
			     (i + spill_args) * sizeof(int32_t));
		else
			emit("  sw a%d, %lu(sp)\n", i - T_MAX,
			     (i + spill_args) * sizeof(int32_t));

	/* prepare callee arguments */
	for (uint32_t i = 0; i < args->len; ++i)
	{
		struct variant_t arg = raw_value(args->buffer[i]);

		if (i < A_MAX)
		{
			opnd(&arg);
			emit("  mv a%c, ", '0' + i);
			next();
			emit("\n");
		}
		else
		{
			uint32_t offset = (i - A_MAX) * sizeof(int32_t);

			opnd(&arg);
			emit("  sw ");
			next();
			emit(", %u(sp)\n", offset);
		}
	}

	emit("  call %s\n", callee->name + 1);

	/* void functions */
	if (callee->ty->data.function.ret->tag == KOOPA_RTT_UNIT)
		return;

	/* functions that have a return value */
	if (m_bb.out_idx < T_MAX)
		emit("  mv t%d, a0\n", m_bb.out_idx);
	else if (m_bb.out_idx < R_MAX - regst_args)
		emit("  mv a%d, a0\n", m_bb.out_idx - T_MAX + m_fn.arg_count);
	else
	{
		emit("  mv t5, a0\n");
		emit("  sw t5, %lu(sp)\n", m_out_sp);
	}

	/* pop saved registers */
	for (uint32_t i = 0; i < min(m_bb.out_idx, R_MAX); ++i)
		if (i < T_MAX)
			emit("  lw t%d, %lu(sp)\n", i,
			     (i + spill_args) * sizeof(int32_t));
		else
			emit("  lw a%d, %lu(sp)\n", i - T_MAX,
			     (i + spill_args) * sizeof(int32_t));
}

/* weird return type... i wonder if there's a better way to backtrack. currently
 * if we're not using `struct variant_t` we'll have to call `htable_lookup()`
 * twice: first in this function, then in `opnd()`.
 * TODO this devastatingly needs to be refactored */
static struct variant_t raw_value(koopa_raw_value_t raw)
{
	if (!raw)
		return (struct variant_t) { .tag = NONE, };

	/* variables, arguments on the stack */
	uint32_t *stack_it = htable_lookup(m_ht_stacks, raw);
	if (stack_it)
		return (struct variant_t) { .tag = STACK, .stack = *stack_it, };

	/* instructions */
	uint32_t *out_it = htable_lookup(m_ht_outs,
					 make_pair(raw, m_fn.bb_idx));
	if (out_it)
		return (struct variant_t) { .tag = OUT, .out = *out_it, };

	raw_type(raw->ty);

	switch (raw->kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		/* immediate */
		break;
	case KOOPA_RVT_ZERO_INIT:
		/* immediate */
		break;
	case KOOPA_RVT_UNDEF:
		todo();
		break;
	case KOOPA_RVT_FUNC_ARG_REF:
		/* stack */
		break;
	case KOOPA_RVT_BLOCK_ARG_REF:
		todo();
		break;
	case KOOPA_RVT_AGGREGATE:
		todo();
		break;
	case KOOPA_RVT_ALLOC:
		/* stack */
		break;
	case KOOPA_RVT_GLOBAL_ALLOC:
		return (struct variant_t) { .tag = GLOBAL, .global = raw };
		break;
	case KOOPA_RVT_LOAD:
		raw_kind_load(&raw->kind.data.load);
		htable_insert(m_ht_outs, make_pair(raw, m_fn.bb_idx),
			      m_bb.out_idx);
		break;
	case KOOPA_RVT_STORE:
		raw_kind_store(&raw->kind.data.store);
		break;
	case KOOPA_RVT_GET_PTR:
		todo();
		break;
	case KOOPA_RVT_GET_ELEM_PTR:
		todo();
		break;
	case KOOPA_RVT_BINARY:
		raw_kind_binary(&raw->kind.data.binary);
		htable_insert(m_ht_outs, make_pair(raw, m_fn.bb_idx),
			      m_bb.out_idx);
		break;
	case KOOPA_RVT_BRANCH:
		raw_kind_branch(&raw->kind.data.branch);
		break;
	case KOOPA_RVT_JUMP:
		raw_kind_jump(&raw->kind.data.jump);
		break;
	case KOOPA_RVT_CALL:
		raw_kind_call(&raw->kind.data.call);
		htable_insert(m_ht_outs, make_pair(raw, m_fn.bb_idx),
			      m_bb.out_idx);
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
#if 0
	raw_slice(&raw->params);
	raw_slice(&raw->used_by);
#endif
	raw_slice(&raw->insts);

	++m_fn.bb_idx;
	memset(&m_bb, 0, sizeof(m_bb));
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

	function_prologue(raw);
	raw_type(raw->ty);
#if 0
	raw_slice(&raw->params);
#endif
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

static void init_values(koopa_raw_slice_t *values)
{
	for (uint32_t i = 0; i < values->len; ++i)
	{
		koopa_raw_value_t value = values->buffer[i];

		assert(value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC);
		koopa_raw_global_alloc_t *kind = &value->kind.data.global_alloc;

		emit("  .globl %s\n", value->name + 1);
		emit("%s:\n", value->name + 1);
		if (kind->init->kind.tag == KOOPA_RVT_ZERO_INIT)
			emit("  .zero 4\n");
		else
		{
			assert(kind->init->kind.tag == KOOPA_RVT_INTEGER);
			emit("  .word %d\n",
			     kind->init->kind.data.integer.value);
		}
	}
}

/* public defn.s */
void codegen(const koopa_raw_program_t *program, FILE *output)
{
	assert(output);

	m_output = output;
	m_ht_outs = htable_ppuu32_new();
	m_ht_stacks = htable_ptru32_new();

	koopa_raw_slice_t *values = &program->values;
	emit("  .data\n");
	init_values(values);

	koopa_raw_slice_t *funcs = &program->funcs;
	emit("\n  .text\n");
	raw_slice(funcs);

	htable_ptru32_delete(m_ht_stacks);
	htable_ppuu32_delete(m_ht_outs);
}
