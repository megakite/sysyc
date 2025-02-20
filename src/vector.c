#include <assert.h>
#include <stdlib.h>

#include "vector.h"

struct vector_t *vector_new(size_t capacity)
{
	struct vector_t *new = malloc(sizeof(*new));
	new->size = 0;
	new->capacity = capacity;
	new->data = malloc(sizeof(*new->data) * capacity);

	return new;
}

void vector_delete(struct vector_t *vec)
{
	free(vec->data);
	free(vec);
}

void *vector_push(struct vector_t *vec, void *el)
{
	assert(vec);

	if (++vec->size > vec->capacity)
	{
		vec->capacity = vec->size * 2;
		vec->data = realloc(vec->data,
				    sizeof(*vec->data) * vec->capacity);
	}
	vec->data[vec->size - 1] = el;

	return &vec->data[vec->size - 1];
}

void *vector_pop(struct vector_t *vec)
{
	return vec->data[--vec->size];
}

void *vector_back(const struct vector_t *vec)
{
	return vec->data[vec->size - 1];
}
