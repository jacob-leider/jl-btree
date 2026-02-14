#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "./btree.h"
#include "./printutils.h"
#include "./btree_print.h"
#include "./btree_tests.h"

#define btree_keep_unused_mem_clean

int RunTests() {

  if (!TestBTreeNodeInsertImpl()) { return 0; }

  if (!TestBTreeNodeDeleteImpl()) { return 0; }

  return 1;
}


int main() {

  if (!RunTests()) { return 1; }
  
 

  return 0;
}
