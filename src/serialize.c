#include "./serialize.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree.h"
#include "./btree_node.h"
#include "./btree_print.h"
#include "./printutils.h"

static int min(int a, int b) { return a <= b ? a : b; }
static int max(int a, int b) { return a <= b ? b : a; }

typedef enum TokenType
{
    LPAREN,
    RPAREN,
    NUMBER,
    ENDTOK
} TokenType;

typedef struct Token
{
    TokenType type;
    int val;
} Token;

typedef struct TokenNode
{
    Token token;
    struct TokenNode* next;
} TokenNode;

typedef struct String
{
    char* str;
    int len;
    int idx;
} String;

/**
 * @brief Tokenize a serialized btree
 *
 * @param s
 * @param len
 *
 * @return The head of a linked list of tokens
 *
 * TODO: Stop using a linked list wtf
 */
TokenNode* tokenize(const char* s, int len)
{
    TokenNode* head = (TokenNode*)malloc(sizeof(TokenNode));
    if (head == NULL)
    {
        printf("TOKENIZATION FAILURE\n");
        return NULL;
    }

    TokenNode* ptr = head;

    int idx        = 0;
    while (idx < len)
    {
        if (s[idx] == '(')
        {
            ptr->token.type = LPAREN;
            idx++;
        }
        else if (s[idx] == ')')
        {
            ptr->token.type = RPAREN;
            idx++;
        }
        else if (isdigit(s[idx]) || s[idx] == '-')
        {
            int sign = 1;
            if (s[idx] == '-')
            {
                sign = -1;
                idx += 1;
                if (!isdigit(s[idx]))
                {
                    printf("TOKENIZATION FAILURE: Can't just put \"-\"\n");
                    return NULL;
                }
            }
            int val = 0;

            while (idx < len && isdigit(s[idx]))
            {
                val *= 10;
                val += s[idx] - '0';
                idx++;
            }

            val *= sign;

            ptr->token.type = NUMBER;
            ptr->token.val  = val;
        }
        else
        {
            idx++;
            continue;
        }

        ptr->next = (TokenNode*)malloc(sizeof(TokenNode));
        if (ptr->next == NULL)
        {
            printf("TOKENIZATION FAILURE\n");
            return NULL;
        }

        ptr = ptr->next;
    }

    ptr->token.type = ENDTOK;

    return head;
}

static int str_inc_size(String* s, int bytes)
{
    if (!s->str)
    {
        s->len = bytes;
        s->str = (char*)malloc(s->len * sizeof(char));
    }
    else
    {
        s->len += bytes;
        s->str = (char*)realloc(s->str, s->len * sizeof(char));
    }

    return s->str != NULL;
}

static int str_append(String* s, char* other, int bytes)
{
    if (!str_inc_size(s, bytes)) return 0;
    memcpy(s->str + s->idx, other, bytes);
    s->idx += bytes;
    return 1;
}

static int str_append_space(String* s) { return str_append(s, " ", 1); }

static int str_append_int(String* s, int n)
{
    char buff[10];
    memset(buff, '\0', 10 * sizeof(char));
    sprintf(buff, "%d", n);
    return str_append(s, buff, strlen(buff));
}

// Helper for `StrFromTree`
static int StrFromTreeR(BTreeNode* root, String* string)
{
    if (!str_append(string, "(", 1)) return 0;

    for (int i = 0; i < root->curr_size; i++)
    {
        if (!btree_node_is_leaf(root))
        {
            BTreeNode* child = btree_node_get_child(root, i);
            if (child == NULL)
            {
                printf("SERIALIZATION ERROR\n");
                printf("child %d of current node is null. Current node:\n", i);
                btree_node_print(root);

                for (int j = i + 1; j <= root->curr_size; j++)
                {
                    BTreeNode* sib = btree_node_get_child(root, j);
                    if (sib == NULL)
                    {
                        printf(" -- Also null: child %d\n", j);
                    }
                    else
                    {
                        printf(" -- Child %d is NOT null: ", j);
                        printArr(sib->keys, sib->curr_size);
                    }
                }

                return 0;
            }

            if (!StrFromTreeR(child, string)) return 0;

            if (btree_node_get_child(root, i) != NULL &&
                !str_append_space(string))
                return 0;
        }

        if (!str_append_int(string, btree_node_get_key(root, i))) return 0;

        if (!btree_node_is_leaf(root))
        {
            BTreeNode* child = btree_node_get_child(root, i + 1);
            if (child == NULL)
            {
                if (child == NULL)
                {
                    printf("SERIALIZATION ERROR\n");
                    printf("child %d of current node is null. Current node:\n",
                        i + 1);
                    btree_node_print(root);

                    for (int j = i + 2; j <= root->curr_size; j++)
                    {
                        BTreeNode* sib = btree_node_get_child(root, j);
                        if (sib == NULL)
                        {
                            printf(" -- Also null: child %d\n", j);
                        }
                        else
                        {
                            printf(" -- Child %d is NOT null: ", j);
                            printArr(sib->keys, sib->curr_size);
                        }
                    }
                    return 0;
                }
            }
        }

        if (i < root->curr_size - 1 || !btree_node_is_leaf(root))
        {
            if (!str_append_space(string)) return 0;
        }
    }

    if (!btree_node_is_leaf(root))
    {
        BTreeNode* child = btree_node_get_last_child(root);
        if (child == NULL)
        {
            if (child == NULL)
            {
                printf("SERIALIZATION ERROR\n");
                printf("child %d of current node is null. Current node:\n",
                    root->curr_size);
                btree_node_print(root);
                return 0;
            }
        }
        if (!StrFromTreeR(child, string)) return 0;
    }

    if (!str_append(string, ")", 1)) return 0;

    return 1;
}

/**
 * @brief Serialize a btree
 *
 * @param root
 *
 * @return 1 on success, 0 on failure
 */
char* StrFromTree(BTreeNode* root)
{
    String s = {NULL, 0, 0};
    if (!StrFromTreeR(root, &s))
    {
        printf("failed to serialize tree\n");
    }
    printf("\n");
    return s.str;
}

/**
 * @brief Generate a btree from serialized format
 *
 * @par Tech notes
 *      - Can 100% be broken with weird syntax. Integers MUST be [-][0-9]+
 *
 * @param str
 * @param len
 * @param node_size maximum node size
 * @param root_ptr
 *
 * @return 1 on success, 0 on failure (it will also scream at you on failure)
 */
int TreeFromStr(const char* str, int len, int node_size, BTreeNode** root_ptr)
{
    BTreeNode* root;
    if (!btree_node_init(node_size, &root, 0))
    {
        return 0;
    }

    TokenNode* head = tokenize(str, len);
    BTreeNode* ptr  = root;
    // TODO: This should really be undefined somehow
    TokenType last_type = ENDTOK;

    while (head != NULL)
    {
        TokenType type = head->token.type;
        int val        = head->token.val;

        if (type == LPAREN)
        {
            if (last_type == RPAREN)
            {
                printf("PARSE ERROR: Children without a separating key\n");
                return 0;
            }

            // Make it internal if it isn't already
            if (btree_node_is_leaf(ptr))
            {
                if (!btree_node_leaf_to_intl(ptr))
                {
                    return 0;
                }
            }

            BTreeNode* child;
            if (!btree_node_init(node_size, &child, 0)) return 0;

            child->par                    = ptr;
            ptr->children[ptr->curr_size] = child;
            ptr                           = child;
        }
        else if (type == RPAREN)
        {
            if (ptr->par == NULL)
            {
                printf("PARSE ERROR: Too many closing parentheses\n");
                return 0;
            }

            // Ascend and add the new child's subtree size to that of the
            // parent.
            int subsize = ptr->subtree_size;
            ptr         = ptr->par;
            ptr->subtree_size += subsize;
        }
        else if (type == NUMBER)
        {
            if (ptr->curr_size == ptr->node_size)
            {
                printf("PARSE ERROR: Too many keys in a node\n");
            }
            ptr->keys[ptr->curr_size] = val;
            ptr->curr_size += 1;
            ptr->subtree_size += 1;
        }
        else
        {
            // Probably fine
        }

        last_type = type;
        head      = head->next;
    }

    // TreeFromStrPopulateVals(root);

    *root_ptr = root;
    return 1;
}

int TreeFromArr(int* vals, int num_vals, int node_size, BTreeNode** root_ptr)
{
    BTreeNode* root;
    if (!btree_node_init(node_size, &root, 1))
    {
        printf("Failed to fill tree (Line %d)\n", __LINE__);
        return 0;
    }

    int vals_idx = 0, lvl_size = 1;
    BTreeNode** lvl = (BTreeNode**)malloc(sizeof(BTreeNode*));
    if (!lvl)
    {
        printf("Mallocn't\n");
        return 0;
    }

    lvl[0] = root;

    while (vals_idx < num_vals)
    {
        // Fill nodes in current lvl
        for (int idx = 0; idx < lvl_size; idx++)
        {
            // fill lvl[i]
            int num_vals_to_cpy = min(num_vals - vals_idx, node_size);

            memcpy(
                lvl[idx]->keys, vals + vals_idx, num_vals_to_cpy * sizeof(int));
            lvl[idx]->curr_size = num_vals_to_cpy;
            vals_idx += num_vals_to_cpy;
        }

        // Add children for this lvl. How many do we need? It's OK if this part
        // runs when num_vals = idx. Make sure num_children_needed is
        // nonnegative so we don't steal the entire heap
        int next_lvl_size = lvl_size * (node_size + 1);
        int num_children_needed =
            max(min((num_vals - vals_idx) / node_size, next_lvl_size), 0);

        // Create next lvl
        BTreeNode** next_lvl =
            (BTreeNode**)malloc(num_children_needed * sizeof(BTreeNode*));
        if (!next_lvl)
        {
            printf("Mallocn't\n");
            return 0;
        }

        int par_idx = 0, key_idx = 0;
        for (int child_idx = 0; child_idx < num_children_needed;
            child_idx++, key_idx++)
        {
            if (key_idx == node_size + 1)
            {
                key_idx = 0;
                par_idx += 1;
            }

            BTreeNode* node;
            if (!btree_node_init(node_size, &node, 1))
            {
                printf("Failed to fill tree (Line %d)\n", __LINE__);
                return 0;
            }

            node->par             = lvl[par_idx];
            next_lvl[child_idx]   = node;
            lvl[par_idx]->is_leaf = 0;  // This should really be moved
            btree_node_set_child(lvl[par_idx], key_idx, node);
        }

        free(lvl);
        lvl      = next_lvl;
        lvl_size = num_children_needed;
    }

    *root_ptr = root;
    return 1;
}
