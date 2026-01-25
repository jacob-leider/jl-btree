#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./btree.h"
#include "./btree_node.h"
#include "./search.h"
#include "./btree_print.h"
#include "./testutils.h"
#include "./printutils.h"

/// Formal-ish Definition of a BTree
///
/// Notes on terminology:
///
///     - "vertex" and "node" are used interchangeably
///
/// A btree of order k is a directed tree whose vertices satisfy the following
/// properties:
///
///     0.0. A vertex can be a "root", a "leaf" or an "internal node". There 
///          are four types of verticies in a btree: root + leaf, 
///          root + internal, leaf, and internal.
///     0.1. Each vertex has a list of increasing integers (`keys`, or 
///          "it's keys" informally). A vertex's "size" is the length of this
///          list.
///     0.2. A vertex's children are totally ordered
///
///     1. There is exactly one root in the tree
///     2. All leaves are on the same level (same distance from the root)
///     3. A leaf has no children
///     4. A vertex has at most k children
///     5. The number of keys must be one less than the number of children for
///        an internal node
///     6. An internal root has at least two children
///     7. An internal node that is not a root has at least t = ceil(k / 2) 
///        children
///     8. The values of the keys of the i-th child of an internal node must be 
///        strictly greater than its (i - 1)-th key.
///     9. The values of the keys of the i-th child of an internal node must be 
///        strictly less than its i-th key.
///    10. A leaf node has at least ceil(k / 2) - 1 keys. 
///
/// These are the commandments of the btree (technically this is Donald Knuth's
/// definition). btree operations (function calls) must preserve these 
/// invariants.
///
/// Implementation Details
///
/// TODO
///
/// Naming Conventions
///
/// 1. If it's for the entire tree rooted at the node, prefix with btree_...
/// 2. If it's just for a btree node, it's btree_node_...
/// 3. If it's just for a btree node, AND that node is a leaf, prefix with
///    btree_node_leaf_...
/// 4. If it's just for a btree node, AND that node is a internal, prefix with
///    btree_node_intl_...


// GENERAL

// @brief Recursively frees all memory referenced by this node assuming all 
// children are uninitialized
//
// TODO: This should't be recursive
//
// @param node
void btree_subtree_kill(BTreeNode* node) {
  if (node == NULL)
    return;
  
  if (node->children) {
    for (int i = 0; i <= node->curr_size; i++)
      btree_subtree_kill(btree_node_get_child(node, i));
  }
  
  btree_node_kill(node);
}


// @brief Determine whether a descendent of `root` contains the key `key`
//
// @param[in] root the root of a btree
// @param[in] key the key we're searching for
//
// @return a return code
//    - 0: tree doesn't contain `key`
//    - 1: tree contains `key`
int btree_node_contains_key(BTreeNode* root, int key) {
  BTreeNode* ptr = root;
  int child_idx = 0;

  // Search for a node containing `key`
  while (!btree_node_is_leaf(ptr)) {
    // Descent logic
    if (btree_node_get_key(ptr, ptr->curr_size - 1) < key) {
      child_idx = ptr->curr_size;
    }
    else if (btree_node_get_key(ptr, ptr->curr_size - 1) == key) {
      return 1;
    }
    else if (btree_node_get_key(ptr, 0) > key) {
      child_idx = 0;
    }
    else {
      child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
      if (btree_node_get_key(ptr, child_idx) == key) return 1;
      child_idx += 1;
    }

    btree_node_intl_descend(&ptr, child_idx);
  } 

  // Check if the leaf contains `key`
  if (btree_node_get_key(ptr, binary_search(ptr->keys, 0, ptr->curr_size, key)) == key) {
    return 1;
  }

  return 0;
}


// @brief Finds the leaf of (the tree rooted at) `root` where `key` should be 
// inserted.
//
// @details
//
// @par Assumptions
//    - The tree rooted at `root` does not contain `key`. We check for this, in 
//    the function, but we that check is inteded as a last resort.
//
// @param[in] root root of the tree
// @param[in] key key we're finding
// @param[out] last_nonfull_anc_ptr pointer to the last non-full ancestor of `leaf`
// (including `leaf`)
// consecutive ancestors starting at `leaf` (including `leaf`) that are full 
// @param[in] inc_subtree_sizes flag that determines whether we increase the 
// subtree_size of each ancestor of `leaf` (including `leaf`). TODO: Replace 
// this with a `flags` field 
//
// @return a return code
//    - 0: Error
//    - 1: (success) Didn't find `key`: `node_ptr` points to the leaf where 
//    `key` should be inserted
int btree_node_find_closest_nonfull_anc(
  BTreeNode* root,
  int key,
  BTreeNode** last_nonfull_anc_ptr,
  int inc_subtree_sizes)
{
  BTreeNode* ptr = root;
  BTreeNode* last_nonfull_anc = NULL;
  int child_idx = 0;

  // Search for a node containing `key`
  while (!btree_node_is_leaf(ptr)) {
    // Update output variables
    if (!btree_node_is_full(ptr)) {
      last_nonfull_anc = ptr;
    }
    
    if (inc_subtree_sizes) {
      ptr->subtree_size += 1;
    }

    // Descent logic
    if (btree_node_get_key(ptr, ptr->curr_size - 1) < key) {
      child_idx = ptr->curr_size;
    }
    else if (btree_node_get_key(ptr, ptr->curr_size - 1) == key) {
      goto found_key;
    }
    else if (btree_node_get_key(ptr, 0) > key) {
      child_idx = 0;
    }
    else {
      child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
      if (btree_node_get_key(ptr, child_idx) == key) goto found_key;
      child_idx += 1;
    }

    btree_node_intl_descend(&ptr, child_idx);
  }
  
  // Update output variables one last time
  if (!btree_node_is_full(ptr)) {
    last_nonfull_anc = ptr;
  }

  if (inc_subtree_sizes) {
    ptr->subtree_size += 1;
  }

  // Make sure the leaf doesn't contain `key`
  if (btree_node_get_key(ptr, binary_search(ptr->keys, 0, ptr->curr_size, key)) == key)
    goto found_key;

success:
  *last_nonfull_anc_ptr = last_nonfull_anc;
  return 1;

found_key:
  return 0;
}


// @brief Finds the descendent of `root` and index of `root`'s keys of `key`.
//
// @par details
//
// @par Assumptions
//    - A descendent of `root` contains `key`
//
// @param[in] root root of the tree
// @param[in] key key we're finding
// @param[out] node_ptr pointer to the descendent `node` of `root` containing `key`.
// Set to NULL if the tree doesn't contain `key`. 
// @param[out] key_idx_ptr pointer to the index of `node` where `key` exists
// @param[out] last_over_min_cap_anc_ptr pointer to the last ancestor of `node` with
// more than `t` children
// @param[out] num_min_cap_ancestors_ptr pointer to an integer that is the number of 
// consecutive ancestors starting at `node` with `t` children
// @param[in] inc_subtree_sizes flag that determines whether we increase the 
// subtree_size of each ancestor of `node`. Used for insertion operations. 
// WARNING: This should only be set if we're certain that the tree contains 
// `key`. TODO: Replace this with a `flags` field
//
// @return a return code
//    - 0: Error
//    - 1: Found `key`
int btree_node_find_key(
  BTreeNode* root,
  int key, 
  BTreeNode** node_ptr, 
  int* key_idx_ptr, 
  BTreeNode** last_over_min_cap_anc_ptr,
  int* num_min_cap_ancestors_ptr, 
  int inc_subtree_sizes) 
{
  BTreeNode* ptr = root;
  BTreeNode* last_over_min_cap_anc = NULL;
  int num_min_cap_ancestors = 0;
  int child_idx = 0;

  // Search for a node containing `key`
  while (!btree_node_is_leaf(ptr)) {
    // Update output variables
    if (btree_node_has_min_cap(ptr)) {
      num_min_cap_ancestors += 1;
    }
    else {
      num_min_cap_ancestors = 0;
      last_over_min_cap_anc = ptr;
    }
    
    if (inc_subtree_sizes) {
      ptr->subtree_size += 1;
    }

    // Descent logic
    if (btree_node_get_key(ptr, ptr->curr_size - 1) < key) {
      child_idx = ptr->curr_size;
    }
    else if (btree_node_get_key(ptr, ptr->curr_size - 1) == key) {
      child_idx = ptr->curr_size - 1;
      goto success;
    }
    else if (btree_node_get_key(ptr, 0) > key) {
      child_idx = 0;
    }
    else {
      child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
      if (btree_node_get_key(ptr, child_idx) == key) goto success;
      child_idx += 1;
    }

    btree_node_intl_descend(&ptr, child_idx);
  }
  
  // Update output variables one last time
  if (btree_node_has_min_cap(ptr)) {
    num_min_cap_ancestors += 1;
  }
  else {
    num_min_cap_ancestors = 0;
    last_over_min_cap_anc = ptr;
  }

  if (inc_subtree_sizes) { 
    ptr->subtree_size += 1;
  }

  // Make sure the leaf `node` contains key
  child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
  if (btree_node_get_key(ptr, child_idx) == key) {
    goto success;
  }

key_not_found:
  return 0;

success:
  *node_ptr = ptr;
  *key_idx_ptr = child_idx;
  *num_min_cap_ancestors_ptr = num_min_cap_ancestors;
  *last_over_min_cap_anc_ptr = last_over_min_cap_anc;
  return 1;
}


// @brief Splits a node (subroutine for `btree_node_insert_impl`)
//
// @detals `key` is being inserted into the subtree rooted at `node`. This 
// function splits `node`, storing the right half in the node `rsib_ptr` points
// to and the separation key in `next_key_ptr`.
//
// @par Assumptions
//    - `node` is full
//
// @param[in] node Node being split
// @param[out] rsib Second (right) node `node` is split into
// @paran[out] next_key_ptr Points to the key `node` was split at
// @paran[in] key Key we're inserting into the tree
//
// @return A return code
//    - 0: Error (OOM)
//    - 1: OK
int btree_node_split(BTreeNode* node, BTreeNode** rsib_ptr, int* next_key_ptr, int key) {
  const int size = node->node_size;
  const int l_start = size / 2;
  const int r_start = l_start + 1;

  BTreeNode* rsib;
  if (!btree_node_init(size, &rsib, node->children != NULL))
    return 0;

  *rsib_ptr = rsib;
  *next_key_ptr = btree_node_get_key(node, l_start);

  node->curr_size = l_start;
  rsib->curr_size = size - r_start;

  rsib->subtree_size = rsib->curr_size;

  memcpy(rsib->keys, node->keys + r_start, rsib->curr_size * sizeof(int));
#ifdef btree_keep_unused_mem_clean
  memset(node->keys + l_start, 0, (size - l_start) * sizeof(int));
#endif

  if (!btree_node_is_leaf(node)) {
    for (int i = 0; i <= rsib->curr_size; i++) {
      btree_node_set_child(rsib, i, btree_node_get_child(node, r_start + i)); 
      rsib->subtree_size += btree_node_get_child(rsib, i)->subtree_size;
    }
#ifdef btree_keep_unused_mem_clean
    memset(node->children + l_start + 1, 0, (size - l_start + 1) * sizeof(BTreeNode*));
#endif
  }

  // `node`'s subtree_size was incremented when we searched for the leaf. If 
  // we're inserting `key` into `rsib`, we have to undo this. 
  if (key > *next_key_ptr) {
    rsib->subtree_size += 1;
  }

  node->subtree_size -= rsib->subtree_size + 1; // +1 for separation key

  return 1;
}


// @brief Inserts a key into a btree
//
// @details TODO
//
// The insertion algorithm is described below:
//
//    1. Find the leaf L whose range contains `key`.
//    2. Find the first ancestor A of L that is not full, or create one if all 
//       are full.
//    3. Get child B of A where `key` should be inserted.
//    4. Split B into (B1, B2) at separation key k.
//    5. Insert (k, B2) into A.
//    6. A is set to B1 if k < `key` or B2 otherwise.
//    7. If A is an internal node, go to line 3. 
//    8. [A is a leaf] Insert `key` into A.
//
// Variable names in our implementation reference this algorithm (albeit in 
// lower case).
//
// @param[in] root Root of a btree
// @param[in] val Key to be inserted into the tree
// @param[out] new_root_ptr Pointer to the root of the tree after `val` is 
// inserted
//
// @return A return code
//    - 0: Error
//    - 1: OK
//    - 2: `val` already exists in the subtree with root `root`
int btree_node_insert_impl(BTreeNode* root, int key, BTreeNode** new_root_ptr) {
  // Exit early if we find `key` in the tree.
  if (btree_node_contains_key(root, key))
    return 2;

  // Default: root doesn't change
  *new_root_ptr = root;

  BTreeNode* a = NULL;
  if (!btree_node_find_closest_nonfull_anc(root, key, &a, 1))
    return 0;

  if (a == NULL) {
    // Create a new node containing just `root` since all ancestors are full
    if (!btree_node_init(root->node_size, &a, 1))
      return 0;

    btree_node_set_first_child(a, root);
    a->subtree_size = root->subtree_size;
    *new_root_ptr = a;
  }

  while (!btree_node_is_leaf(a)) {
    // Get child B of A whose range contains `key`
    BTreeNode* b1 = btree_node_get_child(a, find_idx_of_min_key_greater_than_val(a, key));
    BTreeNode* b2 = NULL;

    // Split B into B1, B2, k (B1: lchild, B2: rchild, k: sep_key)
    int k = 0;
    if (!btree_node_split(b1, &b2, &k, key))
      return 0;

    // Insert (k, B2) into A
    btree_node_insert_key_and_child_assuming_not_full(a, k, b2);

    // Descend
    a = key < k ? b1 : b2;
  }

  btree_node_insert_key_and_child_assuming_not_full(a, key, NULL);

  return 1;
}



// DELETE

// Get the greatest key in the tree ordered before `node->keys[key_idx]` 
// assuming `node` is internal
static void btree_node_get_pred(BTreeNode* node, int key_idx, BTreeNode** pred_leaf_ptr, int* pred_ptr) {
  BTreeNode* ptr = btree_node_get_child(node, key_idx);
  
  while (!btree_node_is_leaf(ptr))
    btree_node_intl_descend(&ptr, ptr->curr_size);

  *pred_leaf_ptr = ptr;
  *pred_ptr = btree_node_get_key(ptr, ptr->curr_size - 1);
}

// @brief Rotate right about `pivot_idx`
//
// @details
//
// @par Assumptions
static void btree_node_rotate_right(BTreeNode* lsib, BTreeNode* rsib, 
                                    BTreeNode* par, int pivot_idx) {
  // append pivot and `lsib`'s last child to the front of rsib
  // replace pivot with last key of `lsib`
  int lsib_last_val = 0;

  lsib->curr_size -= 1;
  rsib->curr_size += 1;

  btree_node_pop_back_key(lsib, &lsib_last_val);
  btree_node_push_front_key(rsib, btree_node_get_key(par, pivot_idx));

  lsib->subtree_size -= 1;
  rsib->subtree_size += 1;

  if (!btree_node_is_leaf(lsib)) {
    BTreeNode* lsib_last_child = NULL;

    btree_node_pop_back_child(lsib, &lsib_last_child);
    btree_node_push_front_child(rsib, lsib_last_child);

    lsib->subtree_size -= lsib_last_child->subtree_size;
    rsib->subtree_size += lsib_last_child->subtree_size;
  }

  btree_node_set_key(par, pivot_idx, lsib_last_val);
}


// @brief Rotate left about `pivot_idx`
//
// @details
//
// @par Assumptions
//    - `rsib` has at least t children
static void btree_node_rotate_left(BTreeNode* lsib, BTreeNode* rsib, BTreeNode* par, int pivot_idx) {
  // append pivot and `rsib`'s first child to the back of lsib
  // replace pivot with first key of `rsib`
  int rsib_first_key = 0;

  lsib->curr_size += 1; 
  rsib->curr_size -= 1;

  btree_node_pop_front_key(rsib, &rsib_first_key);
  btree_node_set_last_key(lsib, btree_node_get_key(par, pivot_idx));

  lsib->subtree_size += 1;
  rsib->subtree_size -= 1;

  if (!btree_node_is_leaf(lsib)) {
    BTreeNode* rsib_first_child = NULL;

    btree_node_pop_front_child(rsib, &rsib_first_child);
    btree_node_set_last_child(lsib, rsib_first_child);

    lsib->subtree_size += rsib_first_child->subtree_size;
    rsib->subtree_size -= rsib_first_child->subtree_size;
  }

  btree_node_set_key(par, pivot_idx, rsib_first_key);
}

// @brief merge `lsib` and `rsib`, storing the result in `lsib`
//
// @details
//
// @par Assumptions
//    - `lsib` and `rsib` are either both leaves or both internal
static void btree_node_merge_sibs(BTreeNode* lsib, BTreeNode* rsib, BTreeNode* par, int sep_idx) {
  // the pivot key, then `rsib`'s keys and (if `lsib` is internal) `rsib`'s
  // children are appended to `lsib`
  if (!btree_node_is_leaf(lsib)) {
    for (int i = 0; i <= rsib->curr_size; i++) {
      btree_node_set_child(lsib, lsib->curr_size + 1 + i, btree_node_get_child(rsib, i));
    }
  }

  // key merge
  btree_node_set_key(lsib, lsib->curr_size, btree_node_get_key(par, sep_idx));
  memcpy(lsib->keys + lsib->curr_size + 1, rsib->keys, 
         rsib->curr_size * sizeof(int));

  lsib->curr_size += rsib->curr_size + 1;
  lsib->subtree_size += rsib->subtree_size + 1;

  btree_node_kill(rsib);

  // Parent loses the pivot key, but its subtree size doens't change
  btree_node_remove_key(par, sep_idx);
  btree_node_remove_child(par, sep_idx + 1);
}


// @brief Deletes a `val` from the btree if the tree contains `val`
//
// @details If `val` in a leaf, it is replaced by its predecessor
//
// @param[in] root Root of a btree
// @param[in] val Key to be removed from the tree
// @param[out] new_root_ptr Pointer to the root of the tree after `val` is 
// (maybe) removed 
//
// @return A return code
//    - 0: Error
//    - 1: OK
//    - 2: Not found
int btree_node_delete_impl(BTreeNode* root, int val, BTreeNode** new_root_ptr) {
  // Allows us to skip a more complicated algorithm if we find it. 
  if (!btree_node_contains_key(root, val))
    return 2;

  // Default: root doesn't change
  *new_root_ptr = root;

  // TODO: Currently, find_key doesn't find the leaf we're looking for. We need 
  // a separate function that computes num_min_cap_ancestors for the node containing 
  // the predecessor to the key we're deleting.
  BTreeNode* ptr = NULL;
  int key_idx = 0;
  BTreeNode* last_over_min_cap_anc = NULL;
  int num_min_cap_ancestors = 0, computed_num_min_cap_ancestors = 0;
  int inc_subtree_sizes = 0;
  if (!btree_node_find_key(root, val, &ptr, &key_idx, 
                               &last_over_min_cap_anc, &num_min_cap_ancestors, 
                               inc_subtree_sizes))
    return 0;

  if (!btree_node_is_leaf(ptr)) {
    // if internal, replace with predecessor and delete predecessor from its 
    // leaf node
    BTreeNode* intl = ptr;
    int pred = 0;
    btree_node_get_pred(intl, key_idx, &ptr, &pred);

    btree_node_set_key(intl, key_idx, pred);
    key_idx = ptr->curr_size - 1;
  }

  // Now `ptr` is definitely a leaf. Delete the value at `key_idx` from the leaf
  btree_node_remove_key(ptr, key_idx);

  // Used for rebalancing
  BTreeNode* sib2 = ptr;

  // Correct subtree size for all affected nodes
  while (ptr != NULL) {
    ptr->subtree_size -= 1;
    ptr = ptr->par;
  }

  // Rebalance the tree if the leaf has too few keys
  const int t = btree_node_t(sib2);
  while (sib2->par != NULL && sib2->curr_size < t) {
    // sib2 has too few keys -- we can borrow a key from a sibling or merge with 
    // a sibling
    BTreeNode* sib1 = NULL;
    BTreeNode *sib3 = NULL; 
    BTreeNode *par = sib2->par;
    btree_node_get_sibs(sib2, sib2->child_idx, &sib1, &sib3);
  
    if (sib3 != NULL && sib3->curr_size > t) {
      // right sibling can spare a key
      btree_node_rotate_left(sib2, sib3, par, sib2->child_idx);
    }
    else if (sib1 != NULL && sib1->curr_size > t) {
      // left sibling can spare a key
      btree_node_rotate_right(sib1, sib2, par, sib2->child_idx - 1);
    }
    else {
      // neither sibling can spare a key
      int sep_idx = sib2->child_idx;
      // pick a sibling to merge with
      BTreeNode *lsib = NULL;
      BTreeNode *rsib = NULL;
      if (sib1 != NULL) {
        lsib = sib1;
        rsib = sib2;
        sep_idx -= 1;
      }
      else {
        lsib = sib2;
        rsib = sib3;
      }

      btree_node_merge_sibs(lsib, rsib, par, sep_idx);
    }

    sib2 = par;
  }

  // If this is the root, it is internal and it has only one child, then we 
  // delete it
  if (btree_node_is_root(sib2) 
    && btree_node_is_empty(sib2) 
    && !btree_node_is_leaf(sib2)) {
    *new_root_ptr = btree_node_get_child(sib2, 0);
    btree_node_kill(sib2);
  }

  return 1;
}
