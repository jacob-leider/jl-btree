
#include "./btree_key.h"

int btree_key_cmp(BTreeKey a, BTreeKey b) { return a - b; }

int btree_key_lt(BTreeKey a, BTreeKey b) { return btree_key_cmp(a, b) < 0; }
int btree_key_gt(BTreeKey a, BTreeKey b) { return btree_key_cmp(a, b) > 0; }
int btree_key_eq(BTreeKey a, BTreeKey b) { return btree_key_cmp(a, b) == 0; }
int btree_key_le(BTreeKey a, BTreeKey b) { return btree_key_cmp(a, b) <= 0; }
int btree_key_ge(BTreeKey a, BTreeKey b) { return btree_key_cmp(a, b) >= 0; }
