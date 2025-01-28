#include <stdlib.h>
#include <stdio.h>

#include "node.h"
#include "types.h"

struct node_t *node_new(struct node_data_t data, int capacity)
{
	struct node_t *new = malloc(sizeof(*new) +
				    sizeof(*new->children) * capacity);
	new->data = data;
	new->size = 0;
	new->capacity = capacity;

	return new;
}

/**
 * Add a child to given root.
 * @return The possibly reallocated root, since we are using flexible members.
 */
struct node_t *node_add_child(struct node_t *root, struct node_t *node)
{
	if (!root)
		return NULL;

	if (++root->size > root->capacity)
	{
		root->capacity = root->size * 2;
		root = realloc(root, sizeof(*root) + 
			       sizeof(*root->children) * root->capacity);
	}
	root->children[root->size - 1] = node;

	return root;
}

void node_traverse_post(struct node_t *node, void (*fn)(void *ptr))
{
	if (!node)
		return;

	for (int i = 0; i < node->size; ++i)
		node_traverse_post(node->children[i], fn);

	fn(node);
}

void node_traverse_pre_depth(struct node_t *node,
			     void (*fn)(void *ptr, int depth), int depth)
{
	if (!node)
		return;

	fn(node, depth);

	for (int i = 0; i < node->size; ++i)
		node_traverse_pre_depth(node->children[i], fn, depth + 1);
}

inline void node_delete(struct node_t *node)
{
	node_traverse_post(node, free);
}
