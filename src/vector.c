#include <assert.h>
#include <stdlib.h>

#include "vector.h"

#define _define_vector_methods(type)					    \
struct vector_##type##_t *vector_##type##_new(size_t capacity)		    \
{									    \
	struct vector_##type##_t *new = malloc(sizeof(*new));		    \
	new->size = 0;							    \
	new->capacity = capacity;					    \
	new->data = malloc(sizeof(*new->data) * capacity);		    \
									    \
	return new;							    \
}									    \
									    \
void vector_##type##_delete(struct vector_##type##_t *vec)		    \
{									    \
	if (!vec)							    \
		return;							    \
									    \
	free(vec->data);						    \
	free(vec);							    \
}									    \
									    \
type *vector_##type##_push(struct vector_##type##_t *vec, type el)	    \
{									    \
	assert(vec);							    \
									    \
	if (++vec->size > vec->capacity)				    \
	{								    \
		vec->capacity = vec->size * 2;				    \
		vec->data = realloc(vec->data,				    \
				    sizeof(*vec->data) * vec->capacity);    \
	}								    \
	vec->data[vec->size - 1] = el;					    \
									    \
	return &vec->data[vec->size - 1];				    \
}									    \
									    \
type vector_##type##_pop(struct vector_##type##_t *vec)			    \
{									    \
	return vec->data[--vec->size];					    \
}									    \
									    \
type vector_##type##_back(const struct vector_##type##_t *vec)		    \
{									    \
	return vec->data[vec->size - 1];				    \
}									    \
									    \
struct vector_##type##_t *vector_##type##_init(size_t capacity, ...)        \
{									    \
	va_list args;							    \
	va_start(args, capacity);					    \
									    \
	struct vector_##type##_t *vec = vector_##type##_new(capacity);	    \
	for (size_t i = 0; i < capacity; ++i)				    \
		vec->data[i] = va_arg(args, type);			    \
									    \
	va_end(args);							    \
									    \
	vec->size = vec->capacity;					    \
	return vec;							    \
}									    \
									    \
struct vector_##type##_t *vector_##type##_fill(size_t capacity, type value) \
{									    \
	struct vector_##type##_t *vec = vector_##type##_new(capacity);	    \
									    \
	for (size_t i = 0; i < vec->capacity; ++i)			    \
		vec->data[i] = value;					    \
	vec->size = vec->capacity;					    \
									    \
	return vec;							    \
}

_define_vector_methods(ptr);
_define_vector_methods(u32);
_define_vector_methods(typ);
