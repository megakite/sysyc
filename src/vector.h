/**
 * vector.h
 * Stack-like dynamic array.
 */

#ifndef _VECTOR_H_
#define _VECTOR_H_

struct vector_t {
	size_t size;
	size_t capacity;
	/* not using flexible member here since it's in fact more costly due to
	 * the extra overhead of redirection introduced every time we perform a
	 * `realloc()`. after all this `vector` is more likely to be used as a
	 * standalone object instead of being put in some structs. */
	void **data;
};

struct vector_t *vector_new(size_t capacity);
void vector_delete(struct vector_t *vec);

void *vector_push(struct vector_t *vec, void *el);
void *vector_pop(struct vector_t *vec);
void *vector_back(const struct vector_t *vec);

#endif//_VECTOR_H_
