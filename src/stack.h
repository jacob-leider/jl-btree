#ifndef __BTREE_STACK_H__
#define __BTREE_STACK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_settings.h"

typedef struct Stack
{
    const size_t element_size;
    size_t capacity;
    size_t size;
    uint8_t* data;
} Stack;

size_t stack_size(Stack* stack);

// Public ----------------------------------------------------------------------
Stack* stack_init(size_t element_size, size_t initial_capacity);

void stack_kill(Stack* stack);

bool stack_is_empty(Stack* stack);

void stack_clear(Stack* stack);

bool stack_get_element(Stack* stack, size_t idx, void* element);

bool stack_get_top(Stack* stack, void* element);

bool stack_pop(Stack* stack, void* element);

bool stack_push(Stack* stack, void* element);

#endif