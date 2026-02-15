#ifndef __BTREE_SERIALIZE_H__
#define __BTREE_SERIALIZE_H__

#include <stdbool.h>

typedef struct BTreeNode BTreeNode;

typedef struct LexerSettings
{
    bool enforce_charset_restriction;  // Error on unrecognized characters
    bool enforce_number_syntax_rules;  // Error on badly formatted number
    int enforce_node_size_limit;       // Error on oversized node
} LexerSettings;

typedef struct DeserializationSettings
{
    int node_size;
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
