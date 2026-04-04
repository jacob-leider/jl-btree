#ifndef __BTREE_SETTINGS_H__
#define __BTREE_SETTINGS_H__

#define DEFAULT_CHILD_IDX_CACHE_SIZE 8
#define DEFAULT_SIB_TO_MERGE_WITH_CACHE_SIZE 8

#define BTREE_NODE_NODE_SIZE 3LL

#define DEFAULT_ERROR_LOG_PATH "./err_log.txt"

#define BTREE_DEBUG_1 0

// An internal node should have at least two children, so we choose a number N
// such that a btree for any reasonable use case is expected to have less than N
// keys, then set this to log2(N). A yottabyte (YB) is 2^80 bytes. Global data
// storage is unlikely to hit 1 YB until the mid 2030s, which makes the
// possibility of inserting 2^80 unique keys into a database impossible for the
// forseeable future (this is a VERY conservative estimate since we aren't even
// factoring in the size such a unique key would need to be).
#define BTREE_MAX_DEPTH 80

// TODO: Fix parts where we clear memory
// #define BTREE_KEEP_UNUSED_MEM_CLEAN 1

#endif