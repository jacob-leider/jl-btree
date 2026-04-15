#include "./serialize.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_print.h"
#include "./core/btree.h"
#include "./core/btree_node.h"
#include "./printutils.h"
#include "./stack.h"
#include "./string_slice.h"

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
    bool is_intl;
} Token;

typedef struct TokenNode
{
    Token token;
    struct TokenNode* next;
} TokenNode;

LexerSettings* default_lexer_settings(void)
{
    static LexerSettings settings = {
        .enforce_charset_restriction = true,
        .enforce_node_size_limit     = true,
        .enforce_number_syntax_rules = true,
    };

    return &settings;
}

/************************************************************************/
/*                             SERIALIZE                                */
/************************************************************************/

// Helper for `StrFromTree`
static int StrFromTreeR(BTreeNode* root, StringBuilder* string)
{
    if (!string_builder_append_willy_nilly(string, "(")) return 0;

    for (size_t i = 0; i < btree_node_num_keys(root); i++)
    {
        if (!btree_node_is_leaf(root))
        {
            BTreeNode* child = btree_node_get_child(root, i);

            assert(child != NULL);

            if (!StrFromTreeR(child, string))
            {
                return 0;
            }

            if (!string_builder_append_willy_nilly(string, " "))
            {
                return 0;
            }
        }

        // Extract integer from key
        BTreeKey* key_to_append = btree_node_get_key(root, i);
        int val_to_append       = 0;
        assert(key_to_append->size == sizeof(int));
        memcpy(&val_to_append, key_to_append->data, sizeof(int));

        if (!string_builder_append_int(string, val_to_append))
        {
            return 0;
        }

        if (i < btree_node_num_keys(root) - 1 || !btree_node_is_leaf(root))
        {
            if (!string_builder_append_willy_nilly(string, " "))
            {
                return 0;
            }
        }
    }

    if (!btree_node_is_leaf(root))
    {
        BTreeNode* child = btree_node_get_last_child(root);

        assert(child != NULL);

        if (!StrFromTreeR(child, string)) return 0;
    }

    if (!string_builder_append_willy_nilly(string, ")")) return 0;

    return 1;
}

/**
 * @brief Serialize a btree
 *
 * @param root
 *
 * @return 1 on success, 0 on failure
 */
char* StrFromTree(BTree* tree)
{
    BTreeNode* root  = tree->root;

    StringBuilder* s = string_builder_new();

    if (s == NULL)
    {
        return NULL;
    }

    if (!StrFromTreeR(root, s))
    {
        printf("failed to serialize tree\n");

        return NULL;
    }

    char* out = string_builder_to_c_string(s);

    string_builder_kill(s);

    return out;
}

/************************************************************************/
/*                            DESERIALIZE                               */
/************************************************************************/

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
        .node_size                        = node_size,
        .fail_when_validation_cant_happen = false,
        .lexer_settings                   = default_lexer_settings(),
    };

    return settings;
}

typedef struct ParseContext
{
    size_t depth;
} ParseContext;

/**
 * @brief Fill out token fields
 *
 * @param tok_seq Token sequence
 * @param n_tokens Number of tokens (size of tok_seq)
 * @param settings Deserialization settings
 * @param err_msg_ptr Optional error message
 * For issues like OOM, set to false.
 * @return [TODO:return]
 */
bool ProvideParseContext(Token* tok_seq,
    size_t n_tokens,
    DeserializationSettings* settings,
    ParseContext* parse_ctx,
    char** err_msg_ptr)
{
    size_t depth      = 0;
    size_t max_depth  = 0;
    Stack* path_stack = stack_init(sizeof(Token*), 8);

    if (path_stack == NULL)
    {
        *err_msg_ptr = "OOM - couldn't initialize path stack";
        return false;
    }

    int last_val           = INT_MIN;

    bool enforce_key_order = settings->lexer_settings->enforce_key_order;

    for (size_t idx = 0; idx < n_tokens; idx++)
    {
        Token* tok = &tok_seq[idx];

        if (tok->type == LPAREN)
        {
            depth += 1;
            max_depth = max(max_depth, depth);

            if (!stack_is_empty(path_stack))
            {
                // Parent is (obviously) not a leaf
                Token* par_tok = NULL;
                stack_get_top(path_stack, &par_tok);

                assert(par_tok != NULL);
                assert(par_tok->type == LPAREN);

                par_tok->is_intl = true;
            }

            stack_push(path_stack, &tok);
        }
        else if (tok->type == RPAREN)
        {
            assert(!stack_is_empty(path_stack));

            stack_pop(path_stack, NULL);
            depth -= 1;
        }
        else if (tok->type == NUMBER)
        {
            // Nothing to do for now.
            int val = tok->val;
            if (enforce_key_order && val < last_val)
            {
                *err_msg_ptr = "Invalid key order";
                return false;
            }
        }
    }

    parse_ctx->depth = max_depth;

    return 1;
}

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
    BTree** tree_ptr)
{
    // Validate settings
    if (settings->node_size < 1)
    {
        printf(
            "Deserialization error. Invalid settings: node_size must be a "
            "positive integer");
        return 0;
    }

    if (settings->lexer_settings == NULL)
    {
        settings->lexer_settings = default_lexer_settings();
    }

    int n_tokens   = 0;
    char* err      = NULL;
    Token* tok_seq = NULL;
    if (!tokenize_tree_str(str, len, &tok_seq, &n_tokens,
            settings->lexer_settings, settings->node_size, &err))
    {
        printf("Deserialization error. Details: \t- %s\n", err);
        return 0;
    }

    ParseContext parse_ctx;
    if (!ProvideParseContext(tok_seq, n_tokens, settings, &parse_ctx, &err))
    {
        return 0;
    }

    BTree* tree = btree_init(settings->node_size);
    if (tree == NULL)
    {
        return 0;
    }

    bool is_intl = parse_ctx.depth > 0;

    // btree_init automatically creates the root as a leaf node. We override
    // that here.
    // TODO: Find a cleaner and more efficient way to do this.
    btree_node_kill(tree->root);
    tree->root = NULL;

    if (!btree_node_init(settings->node_size, &tree->root, is_intl))
    {
        free(tree);
        return 0;
    }

    BTreeNode* ptr = tree->root;

    for (int idx = 0; idx < n_tokens; idx++)
    {
        TokenType type = tok_seq[idx].type;
        int val        = tok_seq[idx].val;
        is_intl        = tok_seq[idx].is_intl;

        if (type == LPAREN)
        {
            // Step down
            assert(!btree_node_is_leaf(ptr));

            BTreeNode* child;

            if (!btree_node_init(settings->node_size, &child, is_intl))
            {
                return 0;
            }

            btree_node_set_par(child, ptr);

            if (btree_node_num_children(ptr) > 0)
            {
                BTreeNode* lsib = btree_node_get_last_child(ptr);

                btree_node_set_left_sib(child, lsib);
                btree_node_set_right_sib(lsib, child);
            }

            btree_node_push_back_child(ptr, child);

            ptr = child;
        }
        else if (type == RPAREN)
        {
            // Step up
            if (btree_node_is_root(ptr))
            {
                printf(
                    "Deserialization error. Details:\n\t- Too many closing "
                    "parentheses\n");
                return 0;
            }

            BTreeNode* child = ptr;
            ptr              = btree_node_par(ptr);

            btree_node_inc_subtree_size(ptr, btree_node_subtree_size(child));
        }
        else if (type == NUMBER)
        {
            // Add key
            if (btree_node_is_full(ptr))
            {
                printf("Deserialization error. Details:\n\t- Overfull node\n");

                return 0;
            }

            // TODO: KEY HANDLING
            BTreeKey key = {
                .data = (char*)&val,
                .size = sizeof(int),
            };

            btree_node_push_back_key(ptr, &key);
            // TODO: KEY HANDLING

            btree_node_inc_subtree_size_1(ptr);
        }
        else
        {
            // Probably fine
            // TODO: No???
        }
    }

    *tree_ptr = tree;

    return 1;
}