/**
 * Core definitions of the generic node system.
 */

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

static struct node_t *node_new(enum node_kind_e kind, union node_data_u data,
			       int capacity)
{
	struct node_t *new = malloc(sizeof(*new) +
				    sizeof(*new->_children) * capacity);
	new->kind = kind;
	new->data = data;
	new->_size = 0;
	new->_capacity = capacity;

	return new;
}

/**
 * Add a child to given root.
 * @return The possibly reallocated root, since we are using flexible members.
 */
static struct node_t *node_add_child(struct node_t *root, struct node_t *node)
{
	if (!root)
		return NULL;

	if (++root->_size > root->_capacity) {
		root->_capacity = root->_size * 2;
		root = realloc(root, sizeof(*root) + 
			       sizeof(*root->_children) * root->_capacity);
	}
	root->_children[root->_size - 1] = node;

	return root;
}

static void node_traverse_post(struct node_t *node, void (*fn)(void *ptr))
{
	if (!node)
		return;

	for (int i = 0; i < node->_size; ++i)
		node_traverse_post(node->_children[i], fn);

	fn(node);
}

static void node_traverse_pre_depth(struct node_t *node,
				    void (*fn)(void *ptr, int depth),
				    int depth)
{
	if (!node)
		return;

	fn(node, depth);

	for (int i = 0; i < node->_size; ++i)
		node_traverse_pre_depth(node->_children[i], fn, depth + 1);
}

inline static void node_destroy(struct node_t *node)
{
	node_traverse_post(node, free);
}

#endif//_NODE_H_
