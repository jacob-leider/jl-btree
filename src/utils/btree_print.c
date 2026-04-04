#include "btree_print.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "./core/btree.h"
#include "./core/btree_node.h"
#include "printutils.h"

void btree_node_print(BTreeNode* node)
{
    printf("BTreeNode (%p)\n", (void*)node);
    printf("\tnode type: %s\n", btree_node_is_leaf(node) ? "Leaf" : "Intl");
    printf("\tcapacity:  %llu\n", btree_node_node_size(node));
    printf("\tcurrent:   %lu\n", btree_node_curr_size(node));
    printf("\telements:  ");
    printArr(node->keys, btree_node_curr_size(node));
}
void printNodeVals(BTreeNode* node)
{
    printArr(node->keys, btree_node_curr_size(node));
}

void printNodeKeys(BTreeNode* node)
{
    printArr(node->keys, btree_node_curr_size(node));
}

void btree_node_print_and_point(BTreeNode* node, int pos)
{
    printf("BTreeNode\n");
    printf("\tcapacity: %llu\n", btree_node_node_size(node));
    printf("\tcurrent: %lu\n", btree_node_curr_size(node));
    printf("\telements: ");
    printArr(node->keys, btree_node_curr_size(node));
    printf("\t          ");
    int num_digits = get_num_digits_of_first_n(node->keys, pos, 10) + 2 * pos;
    for (int i = 0; i < num_digits; i++) printf(" ");
    printf("↑\n");
    printf("num_digits: %d\n", num_digits);
}

// Assumes `node` is initialized
int PrintPath(BTreeNode* node, int pathlen, ...)
{
    int* path = (int*)malloc(pathlen * sizeof(int));
    if (!path)
    {
        printf("Mallocn't\n");
        return 1;
    }

    va_list args;
    va_start(args, pathlen);

    for (int idx = 0; idx < pathlen; idx++)
    {
        int key   = va_arg(args, int);
        path[idx] = key;

        node      = node->children[key];
        if (!node)
        {
            printf("Missing key! -- ");
            printArr(path, idx + 1);
            return 0;
        }
    }

    printf("key ");
    printArrNoNl(path, pathlen);
    printf(": ");
    printNodeVals(node);

    return 1;
}
