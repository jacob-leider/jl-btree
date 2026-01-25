#include <string>
#include <stdlib.h>

int build_num(int* digits, int h) {
  int num = 0;
  int d = 10;
  for (int j = h - 1; j >= 0; j--) {
    num += digits[j] * d;
    d *= 10;
  }

  return num;
}

void gen_tree_r(int t, int* digits, int l, int h) {
  if (l == h)
    return;

  if (l > 0) printf("(");
  
  digits[l] = 0;
  gen_tree_r(t, digits, l + 1, h);
  if (l + 1 < h) printf(" ");

  for (int i = 1; i < t; i++) {
    digits[l] = i;
    printf("%d", build_num(digits, h));
    if (l + 1 < h) printf(" ");
    gen_tree_r(t, digits, l + 1, h);
    printf(" ");
  }

  digits[l] = t;
  printf("%d", build_num(digits, h));

  if (l + 1 < h) printf(" ");

  gen_tree_r(t, digits, l + 1, h);

  if (l > 0) printf(")");

  digits[l] = 0;
}

void gen_tree(int t, int h) {
  int* digits = (int*)malloc(h * sizeof(int));
  // TODO
  int l0 = 0;
  gen_tree_r(t, digits, l0, h);
  free(digits);
}

int main() {
  gen_tree(3, 3);
  printf("\n");
}
