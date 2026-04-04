#ifndef __JL_BTREE_MEM_H__
#define __JL_BTREE_MEM_H__

#include <stdlib.h>

#include "./btree_node.h"
#include "./delete.h"
#include "./string.h"

#define DEFAULT_CHILD_HINT_CACHE_SIZE BTREE_MAX_DEPTH

#define DEFAULT_MERGE_HINT_CACHE_SIZE BTREE_MAX_DEPTH

// Consolidate memory management implementation details to this file
void* jl_btree_malloc(size_t size);
void jl_btree_free(void* ptr);

// TODO: Explain
static size_t CHILD_HINT_CACHE_MEM[DEFAULT_CHILD_HINT_CACHE_SIZE] = {0};

void clear_child_hint_cache(void);

// TODO: Explain
// TODO: Use bits instead of "BTreeNodeSib", which should be private to delete.h
static BTreeNodeSib MERGE_HINT_CACHE_MEM[DEFAULT_MERGE_HINT_CACHE_SIZE] = {0};

void clear_merge_hint_cache(void);

// TODO: How do we reliably allocate memory for the actual btree?

#endif /* __JL_BTREE_MEM_H__ */