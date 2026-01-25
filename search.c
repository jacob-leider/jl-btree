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
int binary_search(int* arr, int lo, int hi, int target) {
  if (hi < lo)
    return 0; // Not allowed

  // Loop invariant: arr[lo] <= target < arr[hi]

  while (lo < hi - 1) {
    int mid = (lo + hi) / 2;
    if (target < arr[mid])
      hi = mid;
    else if (arr[mid] <= target)
      lo = mid;
  }

  return lo;
}
