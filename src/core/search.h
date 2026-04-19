#ifndef __BTREE_SEARCH_H__
#define __BTREE_SEARCH_H__

#include <stddef.h>

#include "btree_key.h"

int binary_search(int* arr, size_t lo, size_t hi, int target);

int binary_search_2(unsigned char* arr,
    size_t lo,
    size_t hi,
    unsigned char* target,
    size_t obj_size,
    int (*cmp)(unsigned char*, unsigned char*));

int binary_search_3(BTreeKey* keys,
    size_t lo,
    size_t hi,
    BTreeKey* target,
    BTreeKeyComparator cmp);

#endif
