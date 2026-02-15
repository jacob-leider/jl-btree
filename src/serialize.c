#include "./serialize.h"

#include <ctype.h>
#include <stdbool.h>
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
    ENDTOK,
    TOKTYPE_UNDEFINED
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

LexerSettings* default_lexer_settings()
{
    const static LexerSettings settings = {.enforce_charset_restriction = true,
        .enforce_node_size_limit                                        = true,
        .enforce_number_syntax_rules                                    = true};

    return &settings;
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
    return s.str;
}

/**
 * @brief Prepare a string for tokenization by validating it and determining how
 * many tokens will be generated
 *
 * @param s
 * @param len length of the string `s`
 * @param settings
 * @param err
 *
 * @return number of tokens that will be generated from `s`. 0 on failure.
 */
int validate_string_and_compute_n_tokens(
    const char* s, int len, LexerSettings* settings, int node_size, char** err)
{
    *err                    = NULL;

    int idx                 = 0;
    int n_tokens            = 0;
    int curr_node_size      = 0;
    int depth               = 1;
    TokenType last_tok_type = 0;
    int max_depth           = 1;

    // 1 Validate parentheses
    for (int i = 0; i < len; i++)
    {
        if (s[i] == '(')
        {
            depth += 1;

            max_depth = max(max_depth, depth);
        }
        else if (s[i] == ')')
        {
            if (depth == 0)
            {
                *err = "Invalid parentheses: unmatched \')\'";
                return 0;
            }

            depth -= 1;
        }
    }

    // Parentheses are valid
    const int default_curr_size_stack_size = 8;
    int default_curr_size_stack[8]         = {0};

    int* curr_size_stack                   = default_curr_size_stack;

    // Resize the stack if computed tree depth exceeded our default stack size
    if (max_depth + 1 > default_curr_size_stack_size)
    {
        curr_size_stack = (int*)calloc((max_depth + 1), sizeof(int));
        if (curr_size_stack == NULL)
        {
            *err = "OOM";
            return 0;
        }
    }

    while (idx < len)
    {
        if (s[idx] == '(')
        {
            if (last_tok_type == RPAREN)
            {
                *err = "Invalid token sequence: \")(\"";
                return 0;
            }

            last_tok_type = LPAREN;
            depth++;

            curr_size_stack[depth] = 0;

            n_tokens++;
            idx++;
        }
        else if (s[idx] == ')')
        {
            last_tok_type = RPAREN;
            depth--;
            n_tokens++;
            idx++;
        }
        else if (isdigit(s[idx]) || s[idx] == '-')
        {
            // (Maybe) ensure node doesn't exceed max node size
            if (curr_size_stack[depth] == node_size &&
                settings->enforce_node_size_limit)
            {
                *err = "Oversized node";
                return 0;
            }

            // Fine for the first char in a number token
            if (s[idx] == '-') idx += 1;

            if (!isdigit(s[idx]) && settings->enforce_number_syntax_rules)
            {
                *err = "Invalid number syntax: lone \'-\'";
                return 0;
            }

            // Only '0'-'9' valid until next non-digit char
            while (idx < len)
            {
                if (isdigit(s[idx]))
                {
                    idx++;
                }
                else if (s[idx] == '-')
                {
                    if (settings->enforce_number_syntax_rules)
                    {
                        *err = "Invalid number syntax: \'-\' after a digit";
                        return 0;
                    }

                    idx++;
                }
                else
                {
                    // Only break when we hit a non-number char
                    break;
                }
            }

            last_tok_type = NUMBER;
            curr_size_stack[depth]++;
            n_tokens++;
        }
        else
        {
            if (!isspace(s[idx]) && settings->enforce_charset_restriction)
            {
                *err = "Invalid character encountered";
                return 0;
            }

            idx++;
        }
    }

    return n_tokens;
}

/**
 * @brief Tokenize a serialized btree
 *
 * @param s
 * @param len Length of s
 * @param tok_seq_ptr Pointer to the output token sequence
 * @param n_tokens_ptr Length of the output token sequence
 * @param settings [TODO:parameter]
 * @param err_ptr
 *
 * @return whether the operation succeeded
 */
bool tokenize_tree_str(const char* s,
    int len,
    Token** tok_seq_ptr,
    int* n_tokens_ptr,
    LexerSettings* settings,
    int node_size,
    char** err_ptr)
{
    // Assume failure by default
    *tok_seq_ptr = NULL;

    char* err    = NULL;
    int n_tokens =
        validate_string_and_compute_n_tokens(s, len, settings, node_size, &err);
    *n_tokens_ptr = n_tokens;

    if (err != NULL)
    {
        *err_ptr = err;
        return 0;
    }

    Token* tok_seq = (Token*)malloc(n_tokens * sizeof(Token));
    if (tok_seq == NULL)
    {
        *err_ptr = "Lexer error: OOM";
        return 0;
    }

    int tok_seq_idx = 0;

    int str_idx     = 0;
    while (str_idx < len)
    {
        if (s[str_idx] == '(')
        {
            tok_seq[tok_seq_idx].type = LPAREN;

            str_idx++;

            tok_seq_idx++;
        }
        else if (s[str_idx] == ')')
        {
            tok_seq[tok_seq_idx].type = RPAREN;

            str_idx++;

            tok_seq_idx++;
        }
        else if (isdigit(s[str_idx]) || s[str_idx] == '-')
        {
            int sign = 1;
            if (s[str_idx] == '-')
            {
                sign = -1;
                str_idx += 1;
            }

            int val = 0;

            while (str_idx < len && isdigit(s[str_idx]))
            {
                val *= 10;
                val += s[str_idx] - '0';
                str_idx++;
            }

            val *= sign;

            tok_seq[tok_seq_idx].type = NUMBER;
            tok_seq[tok_seq_idx].val  = val;

            tok_seq_idx++;
        }
        else
        {
            str_idx++;
            continue;
        }
    }

    *tok_seq_ptr = tok_seq;

    return 1;
}

DeserializationSettings defaut_deserialization_settings(int node_size)
{
    const DeserializationSettings settings = {
        .node_size = node_size, .lexer_settings = default_lexer_settings()};

    return settings;
};

/**
 * @brief Deserialize a serialized btree
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
int TreeFromStr(const char* str,
    int len,
    DeserializationSettings* settings,
    BTreeNode** root_ptr)
{
    // Validate settings
    if (settings->node_size < 1)
    {
        printf(
            "Deserialization error. Invalid settings: node_size must be a "
            "positive integer");
        return 0;
    }

    int n_tokens   = 0;
    char* err      = NULL;
    Token* tok_seq = NULL;
    if (!tokenize_tree_str(str, len, &tok_seq, &n_tokens,
            &settings->lexer_settings, settings->node_size, &err))
    {
        printf("Deserialization error. Details: \t- %s\n", err);
        return 0;
    }

    BTreeNode* root = NULL;
    if (!btree_node_init(settings->node_size, &root, 0))
    {
        return 0;
    }

    BTreeNode* ptr = root;
    for (int idx = 0; idx < n_tokens; idx++)
    {
        TokenType type = tok_seq[idx].type;
        int val        = tok_seq[idx].val;

        if (type == LPAREN)
        {
            // Make it internal if it isn't already
            if (btree_node_is_leaf(ptr))
            {
                if (!btree_node_leaf_to_intl(ptr))
                {
                    return 0;
                }
            }

            BTreeNode* child;
            if (!btree_node_init(settings->node_size, &child, 0)) return 0;

            child->par                    = ptr;
            ptr->children[ptr->curr_size] = child;
            ptr                           = child;
        }
        else if (type == RPAREN)
        {
            if (ptr->par == NULL)
            {
                printf(
                    "Deserialization error. Details:\n\t- Too many closing "
                    "parentheses\n");
                return 0;
            }

            ptr->par->subtree_size += ptr->subtree_size;
            ptr = ptr->par;
        }
        else if (type == NUMBER)
        {
            if (ptr->curr_size == ptr->node_size)
            {
                printf("Deserialization error. Details:\n\t- Overfull node\n");
                return 0;
            }

            ptr->keys[ptr->curr_size] = val;
            ptr->curr_size += 1;
            ptr->subtree_size += 1;
        }
        else
        {
            // Probably fine
        }
    }

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
