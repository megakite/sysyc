#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bump.h"
#include "macros.h"

#define ALIGNMENT alignof(max_align_t)
#define aligned(ptr) ((void *)((uintptr_t)(ptr) & -ALIGNMENT))

/* opaque type definition */
struct _bump_t {
	/* well this is uh... kinda hacky. i can't think of any other way to
	 * make these members overlap tho. */
	union {
		void *ptr;
		char buf[];
	};
};

/* exported functions */
bump_t bump_new(size_t size)
{
	struct _bump_t *new = aligned_alloc(sysconf(_SC_PAGESIZE), size);
	new->ptr = new->buf + size;

	return new;
}

void bump_delete(bump_t bump)
{
	free(bump);
}

/* +-----+-----+     +-----+------+------+     +------+------+------+------+
 * | ptr | n/a | ... | n/a | size | data | ... | size | data | size | data |
 * +-----+-----+     +-----+------+------+     +------+------+------+------+
 * low                     <------`ptr` growth------------------------- high */
void *bump_malloc(bump_t bump, size_t size)
{
	assert(bump);

	void *alloc = aligned((char *)bump->ptr - size);
	bump->ptr = (char *)alloc - ALIGNMENT;
	assert((uintptr_t)bump->ptr > (uintptr_t)bump->buf);

	*(size_t *)bump->ptr = size;

	return alloc;
}

void *bump_calloc(bump_t bump, size_t num, size_t size)
{
	assert(bump);

	void *alloc = bump_malloc(bump, num * size);
	memset(alloc, 0, num * size);

	return alloc;
}

void *bump_realloc(bump_t bump, void *ptr, size_t new_size)
{
	assert(bump);
	assert(ptr);

	void *alloc = bump_malloc(bump, new_size);
	memcpy(alloc, ptr, *(size_t *)((char *)alloc - ALIGNMENT));

	return alloc;
}

char *bump_strdup(bump_t bump, char *str)
{
	assert(bump);
	assert(str);

	void *alloc = bump_malloc(bump, strlen(str) + 1);
	strcpy(alloc, str);

	return alloc;
}
