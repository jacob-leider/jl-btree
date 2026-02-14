#ifndef __BTREE_SERIALIZE_H__
#define __BTREE_SERIALIZE_H__

typedef struct BTreeNode BTreeNode;

// Tree builders

int TreeFromArr(int* vals, int num_vals, int node_size, BTreeNode** root_ptr);

int TreeFromStr(const char* str, int len, int node_size, BTreeNode** root_ptr);

// Tree serializers

char* StrFromTree(BTreeNode* root);

#endif
