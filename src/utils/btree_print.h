// Print utilities for BTree

#ifndef __BTREE_PRINT_H__
#define __BTREE_PRINT_H__

#include <stdio.h>

#include "./core/btree_node.h"

void btree_node_print(BTreeNode* node);

void btree_node_print_and_point(BTreeNode* node, int pos);

int PrintPath(BTreeNode* node, int pathlen, ...);

void fprintNodeKeysNoNl(FILE* fp, BTreeNode* node);

void fprintNodeKeys(FILE* fp, BTreeNode* node);

void printNodeKeysNoNl(BTreeNode* node);

void printNodeKeys(BTreeNode* node);

void fprint_btree_key(FILE* fp, BTreeKey* key);

void fprint_btree_key_from_node(FILE* fp, BTreeNode* node, size_t idx);

void print_btree_key(BTreeKey* key);

void print_btree_key_from_node(BTreeNode* node, size_t idx);

#endif
