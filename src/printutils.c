#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./printutils.h"


void printArrNoNl(int* arr, int length) {
  for (int i = 0; i < length - 1; i++) {
    printf("%d, ", arr[i]);
  }
  printf("%d", arr[length - 1]);
}

void printArr(int* arr, int length) {
  printArrNoNl(arr, length);
  printf("\n");
}

void printArrPtrNoNl(void** arr, int length) {
for (int i = 0; i < length - 1; i++) {
    printf("%p, ", arr[i]);
  }
  printf("%p", arr[length - 1]);
}

void printArrPtr(void** arr, int length) {
  printArrPtrNoNl(arr, length);
  printf("\n");
}


// Helper for `print_and_get_num_digits`. Returns the number of digits of `val`
// in base `base.` Technically supports any base, but shouldn't be used for 
// bases not supported by `print_and_get_num_digits`.
static int get_num_digits(int val, unsigned int base) {
  int num_digits = 0;
  if (val == 0)
    return 1;
  if (val < 0)
    num_digits += 1;
  while (val) {
    val /= base;
    num_digits += 1;
  }

  return num_digits;
}


int get_num_digits_of_first_n(int* arr, int n, int base) {
  int sum = 0;
  for (int i = 0; i < n; i++)
  {
    sum += get_num_digits(arr[i], base);
  }
  return sum;
}


// Prints two lines: Line #1 is the first `length` elements of `arr`. Line #2
// is an up-arrow pointing to the first character of the element at index `pos`.
//
// For example, if `arr` points to an array containing the values 1, 2, 3, 4, 5,
// then `printArr(arr, 5, 2)` would result in 
//
// $ 1, 2, 3, 4, 5
// $    ↑
//
// This doesn't depend on the the elements of `arr` being representable by a 
// single character. If `arr` contained the values 143, 23, 25, 8, 5435, then
// `printArr(arr, 5, 2)` would result in 
//
// $ 143, 23, 25, 8, 5435
// $      ↑
//
// Future additions: A parameter `base` will let the caller specify the base
// in which numbers are printed.
void pointToElementInArr(int* arr, int length, int pos) {
  int chars_before_arrow = get_num_digits_of_first_n(arr, pos, 10) + 2 * pos;
  for (int i = 0; i < length; i++) {
    printf("%d, ", arr[i]);
  }
  printf("\n");
  for (int i = 0; i < chars_before_arrow; i++)
    printf(" ");
  printf("↑\n");
}


// -1 refers to the interval (-inf, arr[0]), k >= 0 refers to [arr[k], arr[k + 1])
void pointBetweenElementsInArr(int* arr, int length, int pos) {
  if (pos < -1)
    return; // Not allowed.
  if (pos >= length)
    return; // Not allowed.
  
  if (pos == -1) {
    // TODO: handle special -1 case.
    printf("  ");
    printArr(arr, length);
    printf(" ↑\n");
    return;
  }

  int chars_before_arrow = get_num_digits_of_first_n(arr, pos + 1, 10) // From first n
    + 2 * pos // From spaces and commas
    + 1; // First space
  printArr(arr, length);
  for (int i = 0; i < chars_before_arrow; i++)
    printf(" ");
  printf("↑\n");
}
