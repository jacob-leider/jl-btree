
#include "btree_key.h"

#include <stdbool.h>

int btree_key_cmp(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size);
}

bool btree_key_eq(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size) == 0;
}

bool btree_key_le(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size) <= 0;
}

bool btree_key_lt(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size) < 0;
}

bool btree_key_ge(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size) >= 0;
}

bool btree_key_gt(
    BTreeKey2* a, BTreeKey2* b, int (*cmp)(char*, size_t, char*, size_t))
{
    return cmp(a->data, a->size, b->data, b->size) > 0;
}
