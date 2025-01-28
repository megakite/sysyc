#ifndef _NODE_H_
#define _NODE_H_

#include <stdlib.h>

#include "types.h"

enum node_tag_e {
	NODE_UNKNOWN = 0,
	NODE_AST,
};

struct node_data_t {
	enum node_tag_e tag;
	union {
		struct ast_data_t ast;
	};
};

struct node_t {
	struct node_data_t data;

	int size;
	int capacity;
	struct node_t *children[];
};

struct node_t *node_new(struct node_data_t data, int capacity);

/**
 * Add a child to given root.
 * @return The possibly reallocated root, since we are using flexible members.
 */
struct node_t *node_add_child(struct node_t *root, struct node_t *node);
void node_traverse_post(struct node_t *node, void (*fn)(void *ptr));
void node_traverse_pre_depth(struct node_t *node,
			     void (*fn)(void *ptr, int depth), int depth);
void node_delete(struct node_t *node);

#endif//_NODE_H_
