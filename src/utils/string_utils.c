#include "./string_utils.h"

#include <stddef.h>

#include "../src/core/mem.h"

static char first_nibble_2_hex(unsigned char nibble)
{
    // Assume nibble < 16
    nibble &= 0x0F;

    if (nibble < 10)
    {
        return '0' + nibble;
    }
    else
    {
        return 'A' + (nibble - 10);
    }
}

char* data_2_hex_str(unsigned char* data, size_t data_size)
{
    char* hex_str = (char*)jl_btree_malloc(2 * data_size * sizeof(char));

    if (hex_str == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < data_size; i++)
    {
        unsigned char byte               = data[i];
        hex_str[2 * (data_size - i) - 1] = first_nibble_2_hex(byte);
        hex_str[2 * (data_size - i) - 2] = first_nibble_2_hex(byte >> 4);
    }

    return hex_str;
}