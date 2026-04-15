#ifndef __JL_BTREE_STRING_SLICE_H__
#define __JL_BTREE_STRING_SLICE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct StringSlice
{
    const char* str;
    size_t len;
} StringSlice;

typedef struct StringBuilder
{
    char* str;
    size_t len;
    size_t cap;
} StringBuilder;

void string_builder_init(StringBuilder* s);

void string_builder_kill(StringBuilder* s);

StringBuilder* string_builder_new(void);

bool string_slice_equal(const StringSlice* a, const StringSlice* b);

bool string_builder_inc_size(StringBuilder* s, size_t inc);

bool string_builder_append(
    StringBuilder* s, const char* other, size_t other_size);

// Dangerous
bool string_builder_append_willy_nilly(StringBuilder* s, const char* other);

char* string_builder_to_c_string(StringBuilder* s);

bool string_builder_append_int(StringBuilder* s, int n);

#endif