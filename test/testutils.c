#include "./testutils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/btree_print.h"
#include "./btree.h"
#include "./btree_node.h"
#include "./printutils.h"
// PRINTING

void testutils_init() { srand(10); }

// @brief Check if this peculiar bit of memory is a btree node
//
// @details
//
// @par Assumptions
//    - elements of `node->children` are btree nodes
//
// @return whether `node` is a valid btree node
//    - 0: Invalid
//    - 1: Valid
int btree_node_is_valid_partial(BTreeNode* node, char** err_msg)
{
    if (!btree_node_is_initialized(node))
    {
        *err_msg = "uninitialized";
        return 0;
    }

    if (node->node_size <= 0)
    {
        *err_msg = "negative capacity";
        return 0;
    }

    if (node->curr_size < 0)
    {
        *err_msg = "negative curr_size";
        return 0;
    }

    if (node->curr_size > node->node_size)
    {
        *err_msg = "data overflow";
        return 0;
    }

    for (int i = 1; i < node->curr_size; i++)
    {
        if (btree_node_get_key(node, i) < btree_node_get_key(node, i - 1))
        {
            *err_msg = "unsorted";
            return 0;
        }
    }

    return 1;
}

typedef struct SubtreeSizeTest
{
    int passed;
    int computed_subtree_size;
} SubtreeSizeTest;

SubtreeSizeTest btree_check_subtree_sizes_impl(BTreeNode* root)
{
    SubtreeSizeTest t = {0, 0};
    if (root->is_leaf)
    {
        t.passed                = root->curr_size == root->subtree_size;
        t.computed_subtree_size = root->curr_size;
        return t;
    }

    int computed_subtree_size = root->curr_size;
    for (int i = 0; i <= root->curr_size; i++)
    {
        SubtreeSizeTest t =
            btree_check_subtree_sizes_impl(btree_node_get_child(root, i));
        if (!t.passed)
        {
            // fail (remove this later on)
            BTreeNode* child = btree_node_get_child(root, i);
            printf("(child %d) Expected: %d, Computed: %d\n", i + 1,
                child->subtree_size, t.computed_subtree_size);
            printf("Child: ");
            printArr(child->keys, child->curr_size);
            return t;
        }
        computed_subtree_size += t.computed_subtree_size;
    }

    t.passed                = computed_subtree_size == root->subtree_size;
    t.computed_subtree_size = computed_subtree_size;
    return t;
}

int btree_check_subtree_sizes(BTreeNode* root)
{
    SubtreeSizeTest t = btree_check_subtree_sizes_impl(root);
    // printf("Res - Expected: %d, Computed: %d\n", root->subtree_size,
    // t.computed_subtree_size);
    return t.passed;
}

int btree_size(BTreeNode* root)
{
    if (!root) return 0;

    int size = 0;
    for (int idx = 0; idx <= root->curr_size; idx++)
    {
        size += btree_size(btree_node_get_child(root, idx));
    }

    return size + root->curr_size;
}

static int btree_cmp_r(BTreeNode* a, BTreeNode* b)
{
    if (a == NULL & b == NULL)
    {
        return 1;
    }
    if (a == NULL ^ b == NULL)
    {
        printf("ONLY ONE NODE IS NULL\n");
        if (a == NULL)
        {
            printf("(a): NULL\n");
            printf("(b): NOT NULL:\n");
            btree_node_print(b);
        }
        else
        {
            printf("(a): NOT NULL:\n");
            btree_node_print(a);
            printf("(b): NULL\n");
        }
        return 0;
    }

    // printf("a: ");
    // printArr(a->keys, a->curr_size);
    // printf("b: ");
    // printArr(b->keys, b->curr_size);

    if (a->node_size != b->node_size)
    {
        return 0;
    }

    if (a->curr_size != b->curr_size)
    {
        printf("WRONG CURR SIZE\n");
        return 0;
    }

    if (a->is_leaf != b->is_leaf)
    {
        printf("LEAF VS. NON-LEAF");
        return 0;
    }

    if (a->subtree_size != b->subtree_size)
    {
        printf("This node (a) has subtree size %d:\n\t", a->subtree_size);
        printArr(a->keys, a->curr_size);
        printf("This node (b) has subtree size %d:\n\t", b->subtree_size);
        printArr(b->keys, b->curr_size);

        return 0;
    }

    // Neither are NULL
    for (int idx = 0; idx < a->curr_size; idx++)
    {
        if (!a->is_leaf && !b->is_leaf)
        {
            if (!btree_cmp_r(
                    btree_node_get_child(a, idx), btree_node_get_child(b, idx)))
            {
                return 0;
            }
        }

        if (btree_node_get_key(a, idx) != btree_node_get_key(b, idx))
        {
            printf("DATA IS DIFFERENT\n");
            return 0;
        }
    }

    // last key
    if (!a->is_leaf && !b->is_leaf)
    {
        if (!btree_cmp_r(
                btree_node_get_last_child(a), btree_node_get_last_child(b)))
        {
            return 0;
        }
    }

    return 1;
}

int btree_cmp(BTreeNode* a, BTreeNode* b)
{
    int res = btree_cmp_r(a, b);
    return res;
}

// DE-RECURSIVIZE (A)
static int btree_subtree_in_order_traverse_r(BTreeNode* root)
{
    for (int i = 0; i < root->curr_size; i++)
    {
        if (!root->is_leaf)
            btree_subtree_in_order_traverse_r(btree_node_get_child(root, i));
        printf("%d, ", btree_node_get_key(root, i));
    }

    if (!root->is_leaf)
        btree_subtree_in_order_traverse_r(btree_node_get_last_child(root));

    return 1;
}

// DE-RECURSIVIZE (B)
int btree_subtree_in_order_traverse(BTreeNode* root)
{
    btree_subtree_in_order_traverse_r(root);
    printf("\n");

    return 1;
}
