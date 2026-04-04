#include "./stack.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Soon to be moved out of core

// Static ----------------------------------------------------------------------

size_t stack_size(Stack* stack);

static size_t last_index(Stack* stack) { return stack->size - 1; }

static uint8_t* stack_data(Stack* stack) { return stack->data; }

static void stack_set_data(Stack* stack, uint8_t* data) { stack->data = data; }

static size_t stack_element_size(Stack* stack) { return stack->element_size; }

static void stack_set_size(Stack* stack, size_t size) { stack->size = size; }

static size_t stack_capacity(Stack* stack) { return stack->capacity; }

static void stack_set_capacity(Stack* stack, size_t capacity)
{
    stack->capacity = capacity;
}

static void stack_inc_size_1(Stack* stack) { stack->size += 1; }

static void stack_dec_size_1(Stack* stack) { stack->size -= 1; }

static uint8_t* element_ptr(Stack* stack, size_t idx)
{
    return stack->data + idx * stack->element_size;
}

static bool stack_full(Stack* stack)
{
    return stack_size(stack) == stack_capacity(stack);
}

static bool stack_resize(Stack* stack)
{
    size_t new_capacity = 2 * stack_capacity(stack);
    uint8_t* new_data   = (uint8_t*)realloc(stack_data(stack), new_capacity);

    if (new_data == NULL)
    {
        return false;
    }

    stack_set_data(stack, new_data);
    stack_set_capacity(stack, new_capacity);

    return true;
}
// Static ----------------------------------------------------------------------

// Public ----------------------------------------------------------------------
Stack* stack_init(size_t element_size, size_t initial_capacity)
{
    uint8_t* data = (uint8_t*)calloc(initial_capacity, element_size);
    if (data == NULL)
    {
        return NULL;
    }

    Stack* stack = (Stack*)malloc(sizeof(Stack));

    if (stack == NULL)
    {
        return NULL;
    }

    Stack stack_tmp = {.element_size = element_size,
        .capacity                    = initial_capacity,
        .size                        = 0,
        .data                        = data};

    memcpy(stack, &stack_tmp, sizeof(Stack));

    return stack;
}

void stack_kill(Stack* stack)
{
    free(stack->data);
    free(stack);
}

size_t stack_size(Stack* stack) { return stack->size; }

bool stack_is_empty(Stack* stack) { return stack_size(stack) == 0; }

void stack_clear(Stack* stack)
{
    stack_set_size(stack, 0);
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    memset(stack_data(), 0, stack_element_size(stack) * stack_size(stack));
#endif
}

bool stack_get_element(Stack* stack, size_t idx, void* element)
{
    if (idx < 0 || idx >= stack_size(stack))
    {
        return false;
    }

    memcpy(element, element_ptr(stack, idx), stack_element_size(stack));

    return true;
}

bool stack_get_top(Stack* stack, void* element)
{
    // Redundant check
    if (stack_is_empty(stack))
    {
        return false;
    }

    return stack_get_element(stack, stack_size(stack) - 1, element);
}

bool stack_pop(Stack* stack, void* element)
{
    if (stack_is_empty(stack))
    {
        return false;
    }

    if (element != NULL)
    {
        memcpy(element, element_ptr(stack, last_index(stack)),
            stack_element_size(stack));
    }

    stack_dec_size_1(stack);

    return true;
}

static bool stack_set_element_intl(Stack* stack, size_t idx, void* element)
{
    if (idx < 0 || idx > stack_size(stack))
    {
        return false;
    }

    memcpy(element_ptr(stack, idx), element, stack_element_size(stack));
    return true;
}

bool stack_push(Stack* stack, void* element)
{
    if (stack_full(stack) && !stack_resize(stack))
    {
        return false;
    }

    if (!stack_set_element_intl(stack, stack_size(stack), element))
    {
        return false;
    }

    stack_inc_size_1(stack);

    return true;
}
