// Print utilities for BTree

#ifndef __BTREE_PRINT_H__
#define __BTREE_PRINT_H__

#include "./btree_node.h"

void btree_node_print(BTreeNode* node);

void btree_node_print_and_point(BTreeNode* node, int pos);

void printNodeVals(BTreeNode* node);

void printNodeKeys(BTreeNode* node);

int PrintPath(BTreeNode* node, int pathlen, ...);

#endif
