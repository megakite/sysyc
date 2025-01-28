#include <assert.h>
#include <stdlib.h>

#include "vector.h"

vector_t vector_new(size_t capacity)
{
	struct _vector_t *new = malloc(sizeof(*new));
	new->size = 0;
	new->capacity = capacity;
	new->data = malloc(sizeof(*new->data) * capacity);

	return new;
}

void vector_push(vector_t vec, void *el)
{
	assert(vec);

	if (++vec->size > vec->capacity)
	{
		vec->capacity = vec->size * 2;
		vec->data = realloc(vec->data,
				    sizeof(*vec->data) * vec->capacity);
	}
	vec->data[vec->size - 1] = el;
}

void vector_iterate(vector_t vec, void (*fn)(void *))
{
	assert(vec);

	for (size_t i = 0; i < vec->size; ++i)
		fn(vec->data[i]);
}

inline void vector_delete(vector_t vec)
{
	free(vec->data);
	free(vec);
}
