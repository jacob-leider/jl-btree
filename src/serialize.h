#ifndef __BTREE_SERIALIZE_H__
#define __BTREE_SERIALIZE_H__

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct BTreeNode BTreeNode;

typedef struct LexerSettings
{
    bool enforce_charset_restriction;  // Error on unrecognized characters
    bool enforce_number_syntax_rules;  // Error on badly formatted number
    bool enforce_node_size_limit;      // Error on oversized node
    bool enforce_key_order;            // Error on out of order keys
} LexerSettings;

typedef struct DeserializationSettings
{
    size_t node_size;
    // If validation returns false for ANY reason (including OOM), exit with an
    // error.
    bool fail_when_validation_cant_happen;
    LexerSettings lexer_settings;
} DeserializationSettings;

// Tree deserializers

int TreeFromArr(int* vals, int num_vals, int node_size, BTreeNode** root_ptr);

int TreeFromStr(const char* str,
    int len,
    DeserializationSettings* settings,
    BTreeNode** root_ptr);

// Tree serializers

char* StrFromTree(BTreeNode* root);

#endif
