/**
 * bump.h
 * Simple bump allocator.
 */

#ifndef _BUMP_H_
#define _BUMP_H_

#include <stddef.h>

typedef struct _bump_t *bump_t;

bump_t bump_new(size_t size);
void bump_delete(bump_t bump);

void *bump_malloc(bump_t bump, size_t size);
void *bump_calloc(bump_t bump, size_t num, size_t size);
void *bump_realloc(bump_t bump, void *ptr, size_t new_size);
char *bump_strdup(bump_t bump, char *str);

#endif//_BUMP_H_
