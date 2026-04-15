
#include "./btree_key.h"

#include <string.h>

// TODO
int btree_key_cmp(BTreeKey* a, BTreeKey* b)
{
    int a_int = 0;
    memcpy(&a_int, a->data, sizeof(int));
    int b_int = 0;
    memcpy(&b_int, b->data, sizeof(int));

    return a - b;
}

int btree_key_lt(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) < 0; }
int btree_key_gt(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) > 0; }
int btree_key_eq(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) == 0; }
int btree_key_le(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) <= 0; }
int btree_key_ge(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) >= 0; }
