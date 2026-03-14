#ifndef __JL_TRIE_H__
#define __JL_TRIE_H__

#include <stdbool.h>
#include <stdlib.h>

typedef struct TrieNode
{
    char key;
    struct TrieNode** children;
    size_t num_children;
    bool is_end_of_word;
} TrieNode;

#endif