#ifndef __BTREE_SEARCH_H__
#define __BTREE_SEARCH_H__

int binary_search(int* arr, size_t lo, size_t hi, int target);

int binary_search_2(char* arr,
    size_t lo,
    size_t hi,
    char* target,
    size_t obj_size,
    int (*cmp)(char*, char*));

#endif
