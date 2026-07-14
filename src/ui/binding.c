#include "ui/binding.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "memory/cmemory.h"

static Binder *AllocBinder()
{
    Binder *b = (Binder *)AllocateBytes(sizeof(Binder));
    if (b) MemorySet(b, 0, sizeof(Binder));
    return b;
}

Binder *Binder_Create(BindingType type, void *target, ValidatorFn validator, void *user_data)
{
    Binder *b = AllocBinder();
    if (!b) return NULL;
    b->type = type;
    b->target = target;
    b->validator = validator;
    b->user_data = user_data;
    return b;
}

void Binder_Destroy(Binder *b)
{
    if (!b) return;
    Deallocate((void **)&b, sizeof(Binder));
}

bool Binder_ValidateAndWrite(Binder *b, const char *text)
{
    if (!b || !text) return false;

    // If custom validator provided, use it
    if (b->validator)
    {
        char buf[256];
        // pass a stack buffer to validator for out_value when needed
        // Validator contract: if parsing, write to out_value pointer which points to a buffer of sufficient size
        return b->validator(text, b->target, b->user_data);
    }

    // Default validators based on type
    switch (b->type)
    {
    case BINDING_INT:
    {
        char *end = NULL;
        long v = strtol(text, &end, 10);
        if (end == text) return false;
        if (b->target) *(int *)b->target = (int)v;
        return true;
    }
    case BINDING_FLOAT:
    {
        char *end = NULL;
        float f = strtof(text, &end);
        if (end == text) return false;
        if (b->target) *(float *)b->target = f;
        return true;
    }
    case BINDING_STRING:
    {
        if (!b->target) return false;
        // assume target points to a fixed-size char* buffer
        strncpy((char *)b->target, text, 255);
        ((char *)b->target)[255] = '\0';
        return true;
    }
    default:
        return false;
    }
}


bool ValidatorIntRange(const char *text, void *out_value, void *user_data)
{
    if (!text || !out_value || !user_data)
        return false;
    char *end = NULL;
    long v = strtol(text, &end, 10);
    if (end == text)
        return false;
    int *range = (int *)user_data;  // [0] = min, [1] = max
    if (v < range[0] || v > range[1])
        return false;
    *(int *)out_value = (int)v;
    return true;
}

bool ValidatorIntPositive(const char *text, void *out_value, void *user_data)
{
    if (!text || !out_value)
        return false;
    char *end = NULL;
    long v = strtol(text, &end, 10);
    if (end == text || v <= 0)
        return false;
    *(int *)out_value = (int)v;
    return true;
}

bool ValidatorFloatRange(const char *text, void *out_value, void *user_data)
{
    if (!text || !out_value || !user_data)
        return false;
    char *end = NULL;
    float f = strtof(text, &end);
    if (end == text)
        return false;
    float *range = (float *)user_data;  // [0] = min, [1] = max
    if (f < range[0] || f > range[1])
        return false;
    *(float *)out_value = f;
    return true;
}
