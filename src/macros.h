/**
 * macros.h
 * Useful macros.
 */

#ifndef _MACROS_H_
#define _MACROS_H_

#include <assert.h>
#include <stdio.h>

#define container_of(ptr, type, member)	\
	(type *)((char *)(ptr) - offsetof(type, member))

#define KiB * 1024
#define MiB * 1024 * 1024

/* can't believe that __VA_OPT__ is a C23 feature... */
#define dbg(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)

#define todo()						 \
	do						 \
	{						 \
		fprintf(stderr, "todo: %s\n", __func__); \
		assert(false /* todo */);		 \
	} while (false)

#define unimplemented()						  \
	do							  \
	{							  \
		fprintf(stderr, "unimplemented: %s\n", __func__); \
		assert(false /* unimplemented */);		  \
	} while (false)

#define panic(msg)						   \
	do							   \
	{							   \
		fprintf(stderr, "panic: %s: %s\n", __func__, msg); \
		assert(false /* panic */);			   \
	} while (false)

#define unreachable()						\
	do							\
	{							\
		fprintf(stderr, "unreachable: %s\n", __func__);	\
		assert(false /* unreachable */);		\
	} while (false)

#define max(a,b)			 \
	({				 \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b;	 \
	})

#define min(a,b)			 \
	({				 \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b;	 \
	})

#endif//_MACROS_H_
