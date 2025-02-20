/**
 * view.h
 * Basically a closure with an iterator.
 */

#ifndef _VIEW_H_
#define _VIEW_H_

struct view_t {
	void *begin;
	void *(*next)(struct view_t *this);
	union {
		char *key;
	} /* capture */;
};

inline static void *view_next_null(struct view_t *this)
{
	(void) this;
	return NULL;
}

#endif//_VIEW_H_
