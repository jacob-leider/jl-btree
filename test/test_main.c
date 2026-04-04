#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_tests.h"

#define btree_keep_unused_mem_clean

int RunTests(void)
{
    if (!TestBTreeInsert())
    {
        return 0;
    }

    if (!TestBTreeDelete())
    {
        return 0;
    }

    return 1;
}

int main(void)
{
    if (!RunTests())
    {
        return 1;
    }

    return 0;
}
