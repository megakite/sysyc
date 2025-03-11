/**
 * yield.h
 * Very, very, very loosely implemented stackless `yield` mechanism.
 */

#include <assert.h>
#include <setjmp.h>

#define YIELD_MAX 2

static struct {
	struct {
		int32_t reg_idx;
	} m_bb;
	uint8_t m_yield_end;
	struct variant_t *variant;
} m_yield_vars[YIELD_MAX];

static uint8_t m_yield_begin;
static uint8_t m_yield_end;
static jmp_buf m_yield[YIELD_MAX];
static jmp_buf m_next[YIELD_MAX];

#define def(name) (m_yield_vars[m_yield_end].name = (name))
#define use(name) (m_yield_vars[m_yield_begin].name)

#define yield(expr)						 \
	do							 \
	{							 \
		assert(m_yield_end < YIELD_MAX);		 \
		/* local variables must be placed into other	 \
		 * locations since the stack will be clobbered   \
		 * after return.				 \
		 * see setjmp(3): Undefined Behavior */		 \
		def(m_yield_end);				 \
		if (setjmp(m_yield[m_yield_end]) == 1)		 \
			longjmp(m_next[use(m_yield_end)], expr); \
		++m_yield_end;					 \
		return;						 \
	}							 \
	while (false)

#define next()						    \
	do						    \
	{						    \
		if (m_yield_begin == 0 && m_yield_end == 0) \
			break;				    \
		/* but no need to save here: this function  \
		 * doesn't resume after returning. */	    \
		uint8_t begin = m_yield_begin;		    \
		if (setjmp(m_next[begin]) == 0)		    \
			longjmp(m_yield[begin], 1);	    \
		++m_yield_begin;			    \
		if (m_yield_begin == m_yield_end)	    \
			m_yield_begin = m_yield_end = 0;    \
	}						    \
	while (false)
