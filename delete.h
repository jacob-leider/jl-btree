#ifndef __BTREE_DELETE_H__
#define __BTREE_DELETE_H__

typedef struct BTreeNode BTreeNode;

typedef enum BTreeNodeSib
{
    UNDEFINED,  // Typically indicates an error
    NEITHER,
    LEFT,
    RIGHT
} BTreeNodeSib;

int btree_node_find_closest_over_min_cap_anc(BTreeNode* root,
    int key,
    BTreeNode** last_over_min_cap_anc_ptr,
    int* chain_end_child_idx_ptr,  // TODO: Rename
    BTreeNodeSib** merge_hint_cache_ptr,
    int** child_idx_cache_ptr);

int btree_node_delete_impl_2(
    BTreeNode* root, int val, BTreeNode** new_root_ptr);

#endif