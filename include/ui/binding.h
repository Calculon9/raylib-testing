#ifndef UI_BINDING_H
#define UI_BINDING_H

#include <stdbool.h>
#include <stddef.h>

// Simple data-binding and validation API for UI text inputs.
// A Binder holds a pointer to target data and a converter/validator function.

typedef enum BindingType
{
    BINDING_NONE = 0,
    BINDING_INT,
    BINDING_FLOAT,
    BINDING_STRING,
} BindingType;

typedef bool (*ValidatorFn)(const char *text, void *out_value, void *user_data);

typedef struct Binder
{
    BindingType type;
    void *target; // pointer to bound storage
    ValidatorFn validator; // returns true if valid and writes parsed value to out_value
    void *user_data; // optional validator context
} Binder;

Binder *Binder_Create(BindingType type, void *target, ValidatorFn validator, void *user_data);
void Binder_Destroy(Binder *b);

// Validate text and, on success, write to the target and return true.
bool Binder_ValidateAndWrite(Binder *b, const char *text);

// Common validators for specific constraints
// IntRange: validates that parsed int is within [min, max] inclusive
bool ValidatorIntRange(const char *text, void *out_value, void *user_data);

// IntPositive: validates that parsed int is > 0
bool ValidatorIntPositive(const char *text, void *out_value, void *user_data);

// FloatRange: validates that parsed float is within [min, max] inclusive
bool ValidatorFloatRange(const char *text, void *out_value, void *user_data);

#endif // UI_BINDING_H

