#ifndef _IRGEN_H_
#define _IRGEN_H_

#include "node.h"

/**
 * Generate IR (in text form) of given AST.
 * @return a dynamically allocated buffer of the IR.
 */
char *irgen(const struct node_t *node);

#endif//_IRGEN_H_
