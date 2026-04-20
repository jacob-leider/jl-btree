#ifndef __JL_BTREE_KEY_H__
#define __JL_BTREE_KEY_H__

#include <stddef.h>

typedef struct BTreeKey
{
    size_t size;
    unsigned char* data;
} BTreeKey;

// to string
typedef char* (*BTreeKeyToString)(BTreeKey*);

// comparator
typedef int (*BTreeKeyComparator)(BTreeKey*, BTreeKey*);

int btree_key_cmp(BTreeKey* a, BTreeKey* b);

int btree_key_lt(BTreeKey* a, BTreeKey* b);
int btree_key_gt(BTreeKey* a, BTreeKey* b);
int btree_key_eq(BTreeKey* a, BTreeKey* b);
int btree_key_le(BTreeKey* a, BTreeKey* b);
int btree_key_ge(BTreeKey* a, BTreeKey* b);

char* btree_key_escape_data(BTreeKey* key);

#endif