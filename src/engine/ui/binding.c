#include "ui/binding.h"
#include "math/cvectors.h"
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

// Parse the vector formats accepted by editable UI fields.
static bool ParseVector2d(const char *text, Vector2d *out_vector)
{
    if (!text || !out_vector)
    {
        return false;
    }

    float parsed_x = 0.0f;
    float parsed_y = 0.0f;
    float parsed_magnitude = 0.0f;
    bool valid_parse =
        (sscanf(text, "(%f,%f)", &parsed_x, &parsed_y) == 2) ||
        (sscanf(text, "%f,%f", &parsed_x, &parsed_y) == 2) ||
        (sscanf(text, "(%f)(%f,%f)", &parsed_magnitude, &parsed_x, &parsed_y) == 3);
    if (!valid_parse)
    {
        return false;
    }

    out_vector->x = parsed_x;
    out_vector->y = parsed_y;
    return true;
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
    case BINDING_VECTOR2D:
    {
        Vector2d parsed_vector = {0};
        if (!ParseVector2d(text, &parsed_vector) || !b->target)
        {
            return false;
        }

        *(Vector2d *)b->target = parsed_vector;
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

