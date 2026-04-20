#ifndef __JL_BTREE_PYTHON_BINDING_STRING_UTILS_H__
#define __JL_BTREE_PYTHON_BINDING_STRING_UTILS_H__

#include <stdlib.h>

#include "../../../../src/utils/string_slice.h"

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

char* data_2_hex_str(char* data, size_t data_size)
{
    char* hex_str = (char*)malloc(2 * data_size * sizeof(char));

    if (hex_str == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < data_size; i++)
    {
        unsigned char byte               = (unsigned char)data[i];
        hex_str[2 * (data_size - i) - 1] = first_nibble_2_hex(byte);
        hex_str[2 * (data_size - i) - 2] = first_nibble_2_hex(byte >> 4);
    }

    return hex_str;
}

int compare_data_slices(char* data1, size_t size1, char* data2, size_t size2)
{
    size_t min_size = size1 < size2 ? size1 : size2;

    for (size_t i = 0; i < min_size; i++)
    {
        if (data1[i] != data2[i])
        {
            return (unsigned char)data1[i] - (unsigned char)data2[i];
        }
    }

    // All bytes in the shorter slice are equal to the corresponding bytes in
    // the longer slice. The longer slice is greater if it has any nonzero
    // bytes in its remaining portion.
    if (size1 > size2)
    {
        for (size_t i = min_size; i < size1; i++)
        {
            if (data1[i] != 0)
            {
                return 1;
            }
        }
        return 0;
    }
    else if (size2 > size1)
    {
        for (size_t i = min_size; i < size2; i++)
        {
            if (data2[i] != 0)
            {
                return -1;
            }
        }
        return 0;
    }
    else
    {
        return 0;
    }
}

#endif  // __JL_BTREE_PYTHON_BINDING_STRING_UTILS_H__