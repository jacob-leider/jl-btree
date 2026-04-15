
#include "./string_slice.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void string_builder_init(StringBuilder* s)
{
    if (s != NULL)
    {
        s->str = NULL;
        s->len = 0;
        s->cap = 0;
    }
}

void string_builder_kill(StringBuilder* s)
{
    if (s != NULL)
    {
        if (s->str != NULL)
        {
            free(s->str);
        }

        free(s);
    }
}

StringBuilder* string_builder_new(void)
{
    StringBuilder* out = (StringBuilder*)malloc(sizeof(StringBuilder));
    string_builder_init(out);
    return out;
}

bool string_slice_equal(const StringSlice* a, const StringSlice* b)
{
    if (a->len != b->len)
    {
        return false;
    }

    if (strncmp(a->str, b->str, a->len) != 0)
    {
        return false;
    }

    return true;
}

bool string_builder_equal(const StringSlice* a, const StringSlice* b)
{
    if (a->len != b->len)
    {
        return false;
    }

    if (strncmp(a->str, b->str, a->len) != 0)
    {
        return false;
    }

    return true;
}

bool string_builder_inc_size(StringBuilder* s, size_t inc)
{
    if (s->str == NULL)
    {
        s->cap = inc;
        s->str = (char*)malloc(s->len * sizeof(char));
    }
    else
    {
        s->cap += inc;
        s->str = (char*)realloc(s->str, s->cap * sizeof(char));
    }

    return s->str != NULL;
}

bool string_builder_append(
    StringBuilder* s, const char* other, size_t other_size)
{
    if (s->len + other_size > s->cap)
    {
        if (!string_builder_inc_size(s, other_size))
        {
            return false;
        }
    }

    memcpy(s->str + s->len, other, other_size);

    s->len += other_size;

    return true;
}

bool string_builder_append_willy_nilly(StringBuilder* s, const char* other)
{
    return string_builder_append(s, other, strlen(other));
}

char* string_builder_to_c_string(StringBuilder* s)
{
    char* out = (char*)malloc((s->len + 1)  // For null terminated string
                              * sizeof(char));

    memcpy(out, s->str, s->len * sizeof(char));

    return out;
}

bool string_builder_append_int(StringBuilder* s, int n)
{
    if (log10(n) > 10)
    {
        // Number is too big
        return false;
    }

    char buff[12];  // 12 to account for null terminated c strings and a
                    // possible sign

    memset(buff, '\0', 10 * sizeof(char));

    sprintf(buff, "%d", n);

    return string_builder_append(s, buff, strlen(buff));
}