#include "./mem.h"

#include <stdlib.h>

#include "./btree_settings.h"
#include "./string.h"

void* jl_btree_malloc(size_t size) { return malloc(size); }

void jl_btree_free(void* ptr) { free(ptr); }

void clear_child_hint_cache(void)
{
    memset(CHILD_HINT_CACHE_MEM, 0,
        DEFAULT_CHILD_HINT_CACHE_SIZE * sizeof(size_t));
}

void clear_merge_hint_cache(void)
{
    memset(MERGE_HINT_CACHE_MEM, 0,
        DEFAULT_MERGE_HINT_CACHE_SIZE * sizeof(BTreeNodeSib));
}