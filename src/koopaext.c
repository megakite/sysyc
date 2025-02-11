#pragma clang diagnostic ignored \
	"-Wincompatible-pointer-types-discards-qualifiers"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "koopaext.h"

/* exported bump allocator */
bump_t g_bump;

/* slice operations */
void slice_append(koopa_raw_slice_t *slice, void *item)
{
	if (slice->len == 0)
	{
		slice->len = 1;
		slice->buffer = bump_malloc(g_bump, sizeof(void *));
		slice->buffer[0] = item;
	}
	else
	{
		/* i mean, this thing is definitely not designed for insertion,
		 * right? since it doesn't have a capacity field, thus we have
		 * to realloc() all the time.
		 * well, perhaps all of those _raw_ things aren't meant to be
		 * manipulated by human, you know, they're all const-qualified.
		 * ah, nevermind. */
		++slice->len;
		slice->buffer = bump_realloc(g_bump, slice->buffer, 
					     sizeof(void *) * slice->len);
		slice->buffer[slice->len - 1] = item;
	}
}

koopa_raw_slice_t slice_new(uint32_t len, koopa_raw_slice_item_kind_t kind)
{
	koopa_raw_slice_t slice = { .len = len, .kind = kind, };

	if (len == 0)
		slice.buffer = NULL;
	else
		slice.buffer = bump_malloc(g_bump, sizeof(void *) * len);

	return slice;
}

void slice_iter(koopa_raw_slice_t *slice, void (*fn)(void *))
{
	for (uint32_t i = 0; i < slice->len; ++i)
		fn(slice->buffer[i]);
}

/* raw program memory management */
inline void koopa_raw_program_set_allocator(bump_t bump)
{
	g_bump = bump;
}
