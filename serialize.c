#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./serialize.h"
#include "./btree.h"
#include "./btree_node.h"


static int min(int a, int b) {return a <= b ? a : b; }
static int max(int a, int b) {return a <= b ? b : a; }


struct String {
  char* str;
  int len;
  int idx;
};

typedef struct String String;

static int str_inc_size(String* s, int bytes) {
  if (!s->str) {
    s->len = bytes;
    s->str = (char*)malloc(s->len * sizeof(char));
  }
  else {
    s->len += bytes; 
    s->str = (char*)realloc(s->str, s->len * sizeof(char));
  }

  return s->str != NULL;
}


static int str_append(String* s, char* other, int bytes) {
  if (!str_inc_size(s, bytes))
    return 0;
  memcpy(s->str + s->idx, other, bytes);
  s->idx += bytes;
  return 1;
}


static int str_append_space(String* s) {
  return str_append(s, " ", 1);
}


static int str_append_int(String* s, int n) {
  char buff[10];
  memset(buff, '\0', 10 * sizeof(char));
  sprintf(buff, "%d", n);
  return str_append(s, buff, strlen(buff));
}


// DE-RECURSIVIZE (A)
static int StrFromTreeR(BTreeNode* root, String* string) {
  if (!root)
    return 1;
  
  if (!str_append(string, "(", 1))
    return 0;

  for (int i = 0; i < root->curr_size; i++) {
    if (!StrFromTreeR(btree_node_get_child(root, i), string))
      return 0;

    if (btree_node_get_child(root, i) != NULL && !str_append_space(string))
        return 0;
    
    if (!str_append_int(string, btree_node_get_key(root, i)))
      return 0;

    if ((btree_node_get_child(root, i + 1) != NULL | i < root->curr_size - 1) && !str_append_space(string))
        return 0;
  }

  if (!StrFromTreeR(btree_node_get_last_child(root), string))
    return 0;

  if (!str_append(string, ")", 1))
    return 0;
  
  return 1;
}


char* StrFromTree(BTreeNode* root) {
  String s = {NULL, 0, 0};
  if (!StrFromTreeR(root, &s)) {
    printf("failed to serialize tree\n");
  }
  printf("\n");
  return s.str;
}

// Helper
static int fill_tree_from_str_add_val(const char* str, int val_start, BTreeNode* ptr, char** err_msg) {
  int val = 0;
  if (!sscanf(str + val_start, "%d", &val)) {
    *err_msg = "Invalid number";
    return 0;
  }

  if (ptr->curr_size == ptr->node_size) {
    *err_msg = "Node overflow";
    return 0;
  }

  ptr->curr_size += 1;
  btree_node_set_last_key(ptr, val);

  return 1;
}
static int is_part_of_int(char c){
  return '0' <= c && c <= '9' | c == '-';
}


static int TreeFromStrFailNice(BTreeNode* root, char* err_msg) {
  btree_subtree_kill(root);
  printf("Error: %s\n", err_msg);
  return 0;
}

void TreeFromStrPopulateVals(BTreeNode* root) {
  if (root->is_leaf) {
    root->subtree_size = root->curr_size;
    return;
  }

  for (int i = 0; i <= root->curr_size; i++) {
    TreeFromStrPopulateVals(btree_node_get_child(root, i));
    root->subtree_size += btree_node_get_child(root, i)->subtree_size;
  }

  root->subtree_size += root->curr_size;
  return;
}

// * Can 100% be broken with weird syntax. Keep integer syntax simple: [-][0-9]+
// * Will NOT check btree invariants.
int TreeFromStr(const char* str, int len, int node_size, BTreeNode** root_ptr) {
  const char DESCEND_CHAR = '(';
  const char ASCEND_CHAR = ')';

  char* err_msg = "No info available";
  int in_val = 0;
  int val_start = -1;

  BTreeNode* root;

  // Determine if root is a leaf
  int seen_int = 0;
  int root_is_leaf = 1;
  for (int i = 0; i < len; i++) {
    char c = str[i];
    if (is_part_of_int(c)) {
      seen_int = 1;
    }

    if (c == DESCEND_CHAR) {
      if (seen_int) {
        root_is_leaf = 0;
        break;
      }
    }
  }

  if (!btree_node_init(node_size, &root, !root_is_leaf))
    return TreeFromStrFailNice(root, "Node initialization failed");

  BTreeNode* ptr = root;

  for (int i = 0; i < len; i++) {
    char c = str[i];
    if (is_part_of_int(c)) {
      // Mark the start of this value so we can parse it later
      if (!in_val) {
        in_val = 1;
        val_start = i;
      }
    }
    else {
      if (in_val && !fill_tree_from_str_add_val(str, val_start, ptr, &err_msg))
        return TreeFromStrFailNice(root, err_msg);

      in_val = 0;
      if (c == DESCEND_CHAR) {
        // Create a child and descend
        BTreeNode* child = NULL;
        // Find out if it's a leaf:
        int is_leaf = 1;
        for (int j = 1; i + j < len; j++) {
          if (str[i + j] == DESCEND_CHAR) {
            is_leaf = 0;
            break;
          }
          else if (str[i + j] == ASCEND_CHAR) {
            break;
          }
        }

        if (!btree_node_init(node_size, &child, !is_leaf))
          return 0;

        btree_node_set_last_child(ptr, child);
        ptr->is_leaf = 0;
        child->par = ptr;
        btree_node_intl_descend(&ptr, ptr->curr_size);
      }
      else if (c == ASCEND_CHAR) {
        ptr = ptr->par;
        if (!ptr) 
          return TreeFromStrFailNice(root, "Bad tree (child with no parent)");
      }
    }
  }

  if (in_val && !fill_tree_from_str_add_val(str, val_start, ptr, &err_msg))
    return TreeFromStrFailNice(root, err_msg);

  // populate sizes
  TreeFromStrPopulateVals(root);

  *root_ptr = root;
  return 1;
}




int TreeFromArr(int* vals, int num_vals, int node_size, BTreeNode** root_ptr) {
  BTreeNode* root;
  if (!btree_node_init(node_size, &root, 1)) {
    printf("Failed to fill tree (Line %d)\n", __LINE__);
    return 0;
  }

  int vals_idx = 0, lvl_size = 1;
  BTreeNode** lvl = (BTreeNode**)malloc(sizeof(BTreeNode*));
  if (!lvl) {
    printf("Mallocn't\n");
    return 0;
  }

  lvl[0] = root;

  while (vals_idx < num_vals) {
    // Fill nodes in current lvl
    for (int idx = 0; idx < lvl_size; idx++) {
      // fill lvl[i]
      int num_vals_to_cpy = min(num_vals - vals_idx, node_size);

      memcpy(lvl[idx]->keys, vals + vals_idx, num_vals_to_cpy * sizeof(int));
      lvl[idx]->curr_size = num_vals_to_cpy;
      vals_idx += num_vals_to_cpy;
    }

    // Add children for this lvl. How many do we need? It's OK if this part runs
    // when num_vals = idx. Make sure num_children_needed is nonnegative so we 
    // don't steal the entire heap
    int next_lvl_size = lvl_size * (node_size + 1);
    int num_children_needed = max(min((num_vals - vals_idx) / node_size, next_lvl_size), 0);

    // Create next lvl
    BTreeNode** next_lvl = (BTreeNode**)malloc(num_children_needed * sizeof(BTreeNode*));
    if (!next_lvl) {
      printf("Mallocn't\n");
      return 0;
    }

    int par_idx = 0, key_idx = 0;
    for (int child_idx = 0; child_idx < num_children_needed; child_idx++, key_idx++) {
      if (key_idx == node_size + 1) {
        key_idx = 0;
        par_idx += 1;
      }

      BTreeNode* node;
      if (!btree_node_init(node_size, &node, 1)) {
        printf("Failed to fill tree (Line %d)\n", __LINE__);
        return 0;
      }

      node->par = lvl[par_idx];
      next_lvl[child_idx] = node;
      lvl[par_idx]->is_leaf = 0; // This should really be moved
      btree_node_set_child(lvl[par_idx], key_idx, node);
    }

    free(lvl);
    lvl = next_lvl;
    lvl_size = num_children_needed;
  }

  *root_ptr = root;
  return 1;
}
