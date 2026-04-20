
#include "./btree_key.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool is_nullish(BTreeKey* a)
{
    if (a == NULL)
    {
        return true;
    }

    if (a->data == NULL)
    {
        return true;
    }

    if (a->size == 0)
    {
        return true;
    }

    return false;
}

// ASSUMES LITTLE ENDIAN - TODO: HOW WILL WE DEAL WITH NORMAL KEYS?
static bool contains_a_set_bit(unsigned char* data, size_t size)
{
    for (int i = size - 1; i >= 0; i--)
    {
        if (data[i] != 0)
        {
            return true;
        }
    }

    return false;
}

// ASSUMES LITTLE ENDIAN - TODO: HOW WILL WE DEAL WITH NORMAL KEYS?
static int cmp_data_same_size(unsigned char* a, unsigned char* b, size_t size)
{
    for (int i = size - 1; i >= 0; i--)
    {
        if (a[i] < b[i])
        {
            return -1;
        }
        else if (a[i] > b[i])
        {
            return 1;
        }
    }
    return 0;
}

static int btree_key_default_cmp(BTreeKey* a, BTreeKey* b)
{
    // TODO: Remove
    assert(!is_nullish(a) && !is_nullish(b));

    if (is_nullish(a) && is_nullish(b))
    {
        return 0;
    }

    if (is_nullish(a) && !is_nullish(b))
    {
        return -1;
    }

    if (!is_nullish(a) && is_nullish(b))
    {
        return 1;
    }

    size_t a_size         = a->size;
    size_t b_size         = b->size;

    unsigned char* a_data = a->data;
    unsigned char* b_data = b->data;

    // TODO: Remove
    assert(a_size == b_size);

    if (a_size > b_size && contains_a_set_bit(a_data + b_size, a_size - b_size))
    {
        // If the leading bytes of 'a' contain a set bit, a is larger.
        return 1;
    }

    if (b_size > a_size && contains_a_set_bit(b_data + a_size, b_size - a_size))
    {
        // If the leading bytes of 'a' contain a set bit, a is larger.
        return -1;
    }

    // Same size.
    return cmp_data_same_size(a_data, b_data, a_size);
}

// TODO: Will this need to take an endianness parameter?
int btree_key_cmp(BTreeKey* a, BTreeKey* b)
{
    int res = btree_key_default_cmp(a, b);

    return res;
}

int btree_key_lt(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) < 0; }
int btree_key_gt(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) > 0; }
int btree_key_eq(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) == 0; }
int btree_key_le(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) <= 0; }
int btree_key_ge(BTreeKey* a, BTreeKey* b) { return btree_key_cmp(a, b) >= 0; }