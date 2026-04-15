#include "btree_print.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./core/btree.h"
#include "./core/btree_node.h"
#include "printutils.h"

static int* keys_to_arr(BTreeNode* node)
{
    assert(node != NULL);

    int* out = (int*)malloc(btree_node_num_keys(node) * sizeof(int));

    if (out == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < btree_node_num_keys(node); i++)
    {
        BTreeKey* key = btree_node_get_key(node, i);
        assert(key->size == sizeof(int));
        memcpy(out + i, key->data, sizeof(int));
    }

    return out;
}

void fprint_btree_key(FILE* fp, BTreeKey* key)
{
    assert(fp != NULL);
    assert(key != NULL);
    assert(key->size >= sizeof(int));

    int val = 0;
    memcpy(&val, key->data, sizeof(int));
    fprintf(fp, "%d", val);
}

void fprint_btree_key_from_node(FILE* fp, BTreeNode* node, size_t idx)
{
    assert(node != NULL);
    assert(idx < btree_node_num_keys(node));
    BTreeKey* key = btree_node_get_key(node, idx);
    fprint_btree_key(fp, key);
}

void print_btree_key(BTreeKey* key) { fprint_btree_key(stdout, key); }

void print_btree_key_from_node(BTreeNode* node, size_t idx)
{
    fprint_btree_key_from_node(stdout, node, idx);
}

void btree_node_print(BTreeNode* node)
{
    printf("BTreeNode (%p)\n", (void*)node);
    printf("\tnode type: %s\n", btree_node_is_leaf(node) ? "Leaf" : "Intl");
    printf("\tcapacity:  %llu\n", btree_node_node_size(node));
    printf("\tcurrent:   %lu\n", btree_node_curr_size(node));
    printf("\telements:  ");

    printNodeKeys(node);
}

void fprintNodeKeysNoNl(FILE* fp, BTreeNode* node)
{
    int* keys_arr = keys_to_arr(node);

    if (keys_arr == NULL)
    {
        return;
    }

    fprintArrNoNl(fp, keys_arr, btree_node_curr_size(node));

    free(keys_arr);
}

void fprintNodeKeys(FILE* fp, BTreeNode* node)
{
    int* keys_arr = keys_to_arr(node);

    if (keys_arr == NULL)
    {
        return;
    }

    fprintArrNoNl(fp, keys_arr, btree_node_curr_size(node));

    free(keys_arr);

    fprintf(fp, "\n");
}

void printNodeKeysNoNl(BTreeNode* node) { fprintNodeKeysNoNl(stdout, node); }

void printNodeKeys(BTreeNode* node) { fprintNodeKeys(stdout, node); }

/*
void btree_node_print_and_point(BTreeNode* node, int pos)
{
    printf("BTreeNode\n");
    printf("\tcapacity: %llu\n", btree_node_node_size(node));
    printf("\tcurrent: %lu\n", btree_node_curr_size(node));
    printf("\telements: ");
    printNodeKeys(node);
    printf("\t          ");
    int num_digits = get_num_digits_of_first_n(node->keys, pos, 10) + 2 * pos;
    for (int i = 0; i < num_digits; i++) printf(" ");
    printf("↑\n");
    printf("num_digits: %d\n", num_digits);
}
*/

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
    printNodeKeys(node);

    return 1;
}
