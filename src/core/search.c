#include <stddef.h>

#include "btree_key.h"

// @brief binary search algorithm
//
// @details The invariants of the loop are:
//    - arr[lo] <= target
//    - arr[hi] > target
//
// `key` will contain the index of the greatest value in `arr` less than or
// equal to `target`. Note that this implies that if `arr` contains `target`,
// then `key` will point to the last instance of `target` in `arr`.
//
// @par Assumptions:
//    - `arr` is nondecreasing between `lo` and `hi`
//    - `arr[lo] <= target < arr[hi - 1]`
//
// @param[in] arr
// @param[in] lo array start
// @param[in] hi array end (must be >= lo)
// @param[in] target the value we're searching for
// @param[out] index of the greatest value less than or equal to `target`
//
// @return an error code.
//      - 1: OK
//      - 0: hi < lo
int binary_search(int* arr, size_t lo, size_t hi, int target)
{
    if (hi < lo) return 0;  // Not allowed

    // Loop invariant: arr[lo] <= target < arr[hi]

    while (lo < hi - 1)
    {
        int mid = (lo + hi) / 2;
        if (target < arr[mid])
            hi = mid;
        else if (arr[mid] <= target)
            lo = mid;
    }

    return lo;
}

int binary_search_2(unsigned char* arr,
    size_t lo,
    size_t hi,
    unsigned char* target,
    size_t obj_size,
    int (*cmp)(unsigned char*, unsigned char*))
{
    if (hi < lo) return 0;  // Not allowed

    // Loop invariant: arr[lo] <= target < arr[hi]

    while (lo < hi - 1)
    {
        int mid                = (lo + hi) / 2;

        unsigned char* mid_obj = arr + mid * obj_size;

        if (cmp(target, mid_obj) < 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid;
        }
    }

    return lo;
}

int binary_search_3(BTreeKey* keys,
    size_t lo,
    size_t hi,
    BTreeKey* target,
    BTreeKeyComparator cmp)
{
    if (hi < lo) return 0;  // Not allowed

    // Loop invariant: arr[lo] <= target < arr[hi]

    while (lo < hi - 1)
    {
        int mid           = (lo + hi) / 2;

        BTreeKey* mid_key = keys + mid;

        if (cmp(target, mid_key) < 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid;
        }
    }

    return lo;
}