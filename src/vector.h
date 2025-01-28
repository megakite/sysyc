#ifndef _VECTOR_H_
#define _VECTOR_H_

struct _vector_t {
	size_t size;
	size_t capacity;
	/* not using flexible member here since it's in fact more costly due to
	 * the extra overhead of redirection introduced every time we perform a
	 * `realloc()`. */
	void **data;
};

typedef struct _vector_t *vector_t;

vector_t vector_new(size_t capacity);
void vector_push(vector_t vector, void *el);
void vector_iterate(vector_t vector, void (*fn)(void *));
void vector_delete(vector_t vector);

#endif//_VECTOR_H_
