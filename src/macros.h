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

#define panic(msg)						       \
	do							       \
	{							       \
		fprintf(stderr, "panic: %s from %s\n", msg, __func__); \
		assert(false /* panic */);			       \
	} while (false)

#define unreachable()						\
	do							\
	{							\
		fprintf(stderr, "unreachable: %s\n", __func__);	\
		assert(false /* unreachable */);		\
	} while (false)

#endif//_MACROS_H_
