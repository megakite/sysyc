/**
 * vector.h
 * Stack-like dynamic array.
 */

#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stdint.h>

#define _define_vector_type(type)				   \
struct vector_##type##_t {					   \
	size_t size;						   \
	size_t capacity;					   \
	/* not using flexible member here since it's in fact more  \
	 * costly due to the extra overhead of redirection	   \
	 * introduced every time we perform a `realloc()`. after   \
	 * all this `vector` is more likely to be used as a	   \
	 * standalone object instead of being put in some structs. \
	 */							   \
	type *data;						   \
};								   \
struct vector_##type##_t *vector_##type##_new(size_t capacity);	   \
struct vector_##type##_t *vector_##type##_new(size_t capacity);	   \
void vector_##type##_delete(struct vector_##type##_t *vec);	   \
type *vector_##type##_push(struct vector_##type##_t *vec, type el); \
type vector_##type##_pop(struct vector_##type##_t *vec);	   \
type vector_##type##_back(const struct vector_##type##_t *vec);

typedef void *ptr;
typedef uint32_t u32;

_define_vector_type(ptr);
_define_vector_type(u32);

#endif//_VECTOR_H_
