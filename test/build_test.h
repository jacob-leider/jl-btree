#ifndef __BTREE_BUILD_TEST_H__
#define __BTREE_BUILD_TEST_H__

#include "../src/btree.h"

typedef struct BTreeNodeInsertImplTestCase
{
    char* test_name;
    size_t size;
    char* before;
    char* after;
    BTreeKey key;
    int rc;
} BTreeNodeInsertImplTestCase;

void build_btree_node_insert_impl_test_case(
    int test_num, BTreeNodeInsertImplTestCase* test_case);

#endif