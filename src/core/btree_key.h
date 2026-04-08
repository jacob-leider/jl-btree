#ifndef __JL_BTREE_KEY_H__
#define __JL_BTREE_KEY_H__

#include <stddef.h>

typedef int (*BTreeKeyComparator)(char*, size_t, char*, size_t);

typedef struct BTreeKey2
{
    size_t size;
    char* data;
} BTreeKey2;

int btree_key_cmp(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

bool btree_key_eq(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

bool btree_key_le(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

bool btree_key_lt(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

bool btree_key_ge(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

bool btree_key_gt(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t));

#endif