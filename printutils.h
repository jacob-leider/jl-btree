// General array printing.

#ifndef __PRINTUTILS_GENERAL_H__
#define __PRINTUTILS_GENERAL_H__

// For fancy printing
#define COLOR_BOLD  "\e[1m"
#define COLOR_OFF   "\e[m"
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */


void printArr(int* arr, int length);

void printArrNoNl(int* arr, int length);

void printArrPtr(void** arr, int length);

void printArrPtrNoNl(void** arr, int length);

int get_num_digits_of_first_n(int* arr, int n, int base);


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
void pointToElementInArr(int* arr, int length, int pos);

void pointBetweenElementsInArr(int* arr, int length, int pos);


#endif
