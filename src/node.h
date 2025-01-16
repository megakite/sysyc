#ifndef _NODE_H_
#define _NODE_H_

#include <stdlib.h>

#include "types.h"

enum node_kind_e {
	NODE_UNKNOWN = 0,
	NODE_AST,
};

union node_data_u {
	struct ast_data_t ast_data;
};

struct node_t {
	enum node_kind_e kind;
	union node_data_u data;

	int _size;
	int _capacity;
	struct node_t *_children[];
};

struct node_t *node_new(enum node_kind_e kind, union node_data_u data,
			int capacity);

/**
 * Add a child to given root.
 * @return The possibly reallocated root, since we are using flexible members.
 */
struct node_t *node_add_child(struct node_t *root, struct node_t *node);

void node_traverse_post(struct node_t *node, void (*fn)(void *ptr));

void node_traverse_pre_depth(struct node_t *node,
			     void (*fn)(void *ptr, int depth), int depth);

void node_destroy(struct node_t *node);

#endif//_NODE_H_
