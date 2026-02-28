#include "./testutils.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../src/btree_print.h"
#include "../src/stack.h"
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

    if (btree_node_node_size(node) <= 0)
    {
        *err_msg = "negative capacity";
        return 0;
    }

    if (btree_node_curr_size(node) < 0)
    {
        *err_msg = "negative curr_size";
        return 0;
    }

    if (btree_node_curr_size(node) > btree_node_node_size(node))
    {
        *err_msg = "data overflow";
        return 0;
    }

    for (int i = 1; i < btree_node_curr_size(node); i++)
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
    if (btree_node_is_leaf(root))
    {
        t.passed = btree_node_curr_size(root) == btree_node_subtree_size(root);
        t.computed_subtree_size = btree_node_curr_size(root);
        return t;
    }

    int computed_subtree_size = btree_node_curr_size(root);
    for (int i = 0; i <= btree_node_curr_size(root); i++)
    {
        SubtreeSizeTest t =
            btree_check_subtree_sizes_impl(btree_node_get_child(root, i));
        if (!t.passed)
        {
            // fail (remove this later on)
            BTreeNode* child = btree_node_get_child(root, i);
            printf("(child %d) Expected: %d, Computed: %d\n", i + 1,
                btree_node_subtree_size(child), t.computed_subtree_size);
            printf("Child: ");
            printArr(child->keys, btree_node_curr_size(child));
            return t;
        }
        computed_subtree_size += t.computed_subtree_size;
    }

    t.passed = computed_subtree_size == btree_node_subtree_size(root);
    t.computed_subtree_size = computed_subtree_size;
    return t;
}

int btree_check_subtree_sizes(BTreeNode* root)
{
    SubtreeSizeTest t = btree_check_subtree_sizes_impl(root);
    // printf("Res - Expected: %d, Computed: %d\n",
    // btree_node_subtree_size(root), t.computed_subtree_size);
    return t.passed;
}

int btree_size(BTreeNode* root)
{
    if (!root) return 0;

    int size = 0;
    for (int idx = 0; idx <= btree_node_curr_size(root); idx++)
    {
        size += btree_size(btree_node_get_child(root, idx));
    }

    return size + btree_node_curr_size(root);
}

typedef struct BTreeCmpState
{
    BTreeNode* a_root;
    BTreeNode* b_root;
    BTreeNode* a;
    BTreeNode* b;
    const char* a_name;
    const char* b_name;
    Stack* path_stack;
    FILE* outp_fp;
} BTreeCmpState;

static void print_n(FILE* fp, char c, size_t n)
{
    for (size_t i = 0; i < n; i++) fprintf(fp, "%c", c);
}

static void print_path_from_stack_verbose(BTreeCmpState* state)
{
    BTreeNode* ptr = state->a_root;

    assert(ptr != NULL);

    size_t num_spaces = 4;
    print_n(state->outp_fp, ' ', num_spaces);
    fprintf(state->outp_fp, "(");
    fprintArrNoNl(
        state->outp_fp, btree_node_keys(ptr), btree_node_num_keys(ptr));
    fprintf(state->outp_fp, ")");

    size_t temp = 0;
    for (size_t depth = 0; depth < stack_size(state->path_stack); depth++)
    {
        stack_get_element(state->path_stack, depth, &temp);
        ptr = btree_node_get_child(ptr, temp);

        fprintf(state->outp_fp, "\n");
        print_n(state->outp_fp, ' ', num_spaces);

        fprintf(state->outp_fp, "│");

        fprintf(state->outp_fp, "\n");
        print_n(state->outp_fp, ' ', num_spaces);

        fprintf(state->outp_fp, "└─> (");
        fprintArrNoNl(
            state->outp_fp, btree_node_keys(ptr), btree_node_num_keys(ptr));
        fprintf(state->outp_fp, ")");

        fprintf(state->outp_fp, "    idx = %d", temp);

        num_spaces += 4;
    }

    fprintf(state->outp_fp, "\n");
}

void print_path_from_stack(BTreeCmpState* state)
{
    if (stack_is_empty(state->path_stack))
    {
        fprintf(state->outp_fp, "root");
    }

    size_t temp = 0;
    for (size_t depth = 0; depth < stack_size(state->path_stack); depth++)
    {
        stack_get_element(state->path_stack, depth, &temp);
        fprintf(state->outp_fp, " -> %d", temp);
    }

    fprintf(state->outp_fp, "\n");
}

static void print_horizontal_line(FILE* fp)
{
    struct winsize w;
    unsigned short cols = 80;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
    {
        cols = w.ws_col;
    }

    for (unsigned short i = 0; i < cols; i++) fprintf(fp, "-");
    fprintf(fp, "\n");
}

void btree_cmp_print_fail_start(BTreeCmpState* state)
{
    fprintf(state->outp_fp, "Path:\n\n");
    print_path_from_stack_verbose(state);
}

void btree_cmp_print_fail_end(BTreeCmpState* state) {}

static bool btree_cmp_r_null_check(BTreeCmpState* state)
{
    if (state->a == NULL && state->b == NULL)
    {
        return true;
    }

    if (state->a == NULL || state->b == NULL)
    {
        btree_cmp_print_fail_start(state);

        fprintf(state->outp_fp, "(%s): is null\n",
            state->a == NULL ? state->a_name : state->b_name);
        fprintf(state->outp_fp, "(%s): is not null:\n",
            state->a == NULL ? state->b_name : state->a_name);

        btree_cmp_print_fail_end(state);

        return false;
    }

    return true;
}

static bool btree_cmp_r_size_check(BTreeCmpState* state)
{
    return btree_node_node_size(state->a) == btree_node_node_size(state->b);
}

static bool btree_cmp_r_leaf_check(BTreeCmpState* state)
{
    if (btree_node_is_leaf(state->a) != btree_node_is_leaf(state->b))
    {
        btree_cmp_print_fail_start(state);

        fprintf(state->outp_fp, "Node (%s) is a leaf and node (%s) is not.\n",
            btree_node_is_leaf(state->a) ? state->a_name : state->b_name,
            btree_node_is_leaf(state->a) ? state->b_name : state->a_name);

        btree_cmp_print_fail_end(state);

        return false;
    }

    return true;
}

static bool btree_cmp_r_num_keys_check(BTreeCmpState* state)
{
    if (btree_node_num_keys(state->a) != btree_node_num_keys(state->b))
    {
        btree_cmp_print_fail_start(state);

        fprintf(state->outp_fp, "(%s) has size %d:\n\t", state->a_name,
            btree_node_num_keys(state->a));
        fprintf(state->outp_fp, "(%s) has size %d:\n\t", state->b_name,
            btree_node_num_keys(state->b));

        btree_cmp_print_fail_end(state);

        return false;
    }

    return true;
}

static bool btree_cmp_r_num_children_check(BTreeCmpState* state)
{
    if (btree_node_num_children(state->a) != btree_node_num_children(state->b))
    {
        btree_cmp_print_fail_start(state);

        fprintf(state->outp_fp, "(%s) has %d children:\n\t", state->a_name,
            btree_node_num_children(state->a));
        fprintArr(state->outp_fp, btree_node_keys(state->a),
            btree_node_num_keys(state->a));
        fprintf(state->outp_fp, "(%s) has %d children:\n\t", state->b_name,
            btree_node_num_children(state->b));
        fprintArr(state->outp_fp, btree_node_keys(state->b),
            btree_node_num_keys(state->b));

        btree_cmp_print_fail_end(state);

        return false;
    }

    return true;
}

static bool btree_cmp_r_subtree_size_check(BTreeCmpState* state)
{
    if (btree_node_subtree_size(state->a) != btree_node_subtree_size(state->b))
    {
        fprintf(state->outp_fp, "Failed check (s)\n\n  - Subtree size\n");
        fprintf(state->outp_fp, "\n");

        fprintf(state->outp_fp, "Path:\n\n");
        print_path_from_stack_verbose(state);

        fprintf(state->outp_fp, "\n");
        fprintf(state->outp_fp, "Details\n\n");

        fprintf(state->outp_fp, "  - Subtree size of (%s) is %d\n",
            state->a_name, btree_node_subtree_size(state->a));
        fprintf(state->outp_fp, "  - Subtree size of (%s) is %d\n",
            state->b_name, btree_node_subtree_size(state->b));

        btree_cmp_print_fail_end(state);

        return false;
    }

    return true;
}

static bool btree_cmp_r(BTreeCmpState* state)
{
    BTreeNode* a       = state->a;
    BTreeNode* b       = state->b;
    const char* a_name = state->a_name;
    const char* b_name = state->b_name;

    assert(a != NULL);
    assert(b != NULL);

    if (!btree_cmp_r_null_check(state))
    {
        return false;
    }

    if (!btree_cmp_r_size_check(state))
    {
        return false;
    }

    if (!btree_cmp_r_leaf_check(state))
    {
        return false;
    }

    if (!btree_cmp_r_num_keys_check(state))
    {
        return false;
    }

    if (!btree_cmp_r_num_children_check(state))
    {
        return false;
    }

    if (!btree_cmp_r_subtree_size_check(state))
    {
        return false;
    }

    // Neither are NULL
    for (size_t idx = 0; idx < btree_node_curr_size(state->a); idx++)
    {
        if (!btree_node_is_leaf(state->a) && !btree_node_is_leaf(state->b))
        {
            stack_push(state->path_stack, &idx);
            state->a = btree_node_get_child(a, idx);
            state->b = btree_node_get_child(b, idx);

            if (!btree_cmp_r(state))
            {
                return false;
            }

            stack_pop(state->path_stack, NULL);
            state->a = a;
            state->b = b;
        }
    }

    // last key
    if (!btree_node_is_leaf(state->a) && !btree_node_is_leaf(state->b))
    {
        size_t idx = btree_node_curr_size(state->a);

        stack_push(state->path_stack, &idx);
        state->a = btree_node_get_child(a, idx);
        state->b = btree_node_get_child(b, idx);

        if (!btree_cmp_r(state))
        {
            return false;
        }

        stack_pop(state->path_stack, NULL);
        state->a = a;
        state->b = b;
    }

    return true;
}

bool btree_cmp(BTreeCmpSettings* settings)
{
    size_t initial_path_stack_size = 8;
    Stack* path = stack_init(sizeof(size_t), initial_path_stack_size);

    assert(settings != NULL);
    assert(settings->a_root != NULL);
    assert(settings->b_root != NULL);

    if (settings->a_name == NULL)
    {
        settings->a_name = "a";
    }

    if (settings->b_name == NULL)
    {
        settings->b_name = "b";
    }

    // TODO: Better default files (location, name, etc.)
    const char* default_log_file_path = "./err_log.txt";

    if (settings->log_file_path == NULL)
    {
        settings->log_file_path = default_log_file_path;
    }

    FILE* fp = fopen(settings->log_file_path, "w");

    if (fp == 0)
    {
        fp = fopen(default_log_file_path, "w");
    }

    BTreeCmpState state = {.a = settings->a_root,
        .b                    = settings->b_root,
        .a_root               = settings->a_root,
        .b_root               = settings->b_root,
        .a_name               = settings->a_name,
        .b_name               = settings->b_name,
        .path_stack           = path,
        .outp_fp              = fp};

    bool res            = btree_cmp_r(&state);

    fclose(state.outp_fp);

    return res;
}

// DE-RECURSIVIZE (A)
static int btree_subtree_in_order_traverse_r(BTreeNode* root)
{
    for (size_t i = 0; i < btree_node_curr_size(root); i++)
    {
        if (!btree_node_is_leaf(root))
            btree_subtree_in_order_traverse_r(btree_node_get_child(root, i));
        printf("%d, ", btree_node_get_key(root, i));
    }

    if (!btree_node_is_leaf(root))
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
