#include "ui/binding.h"
#include "math/cvectors.h"
#include "system/systems.h" // PipelineNumberToText / PipelineVectorToText (the leaf format primitives)
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

//------------------------------------------------------------------------------------------
// Symmetric binding core
//------------------------------------------------------------------------------------------

// FORMAT: value -> text buffer. Dispatches by type to the leaf format primitives so there is
// exactly ONE home per format string (INT "%d", FLOAT "%.<precision>f", VECTOR2D "(%.2f,%.2f)").
bool Binding_FormatValue(BindingValue value, int precision, char *out, size_t out_bytes)
{
    if (!out || out_bytes == 0)
    {
        return false;
    }

    switch (value.type)
    {
    case BIND_INT:
        snprintf(out, out_bytes, "%d", value.as.i);
        return true;
    case BIND_FLOAT:
        PipelineNumberToText(value.as.f, precision, out, out_bytes);
        return true;
    case BIND_VECTOR2D:
        PipelineVectorToText(value.as.v, out, out_bytes);
        return true;
    case BIND_STRING:
    {
        // Bounded copy of a non-owning source string into the caller buffer.
        if (!value.as.s)
        {
            return false;
        }
        strncpy(out, value.as.s, out_bytes - 1);
        out[out_bytes - 1] = '\0';
        return true;
    }
    case BIND_NONE:
    default:
        return false;
    }
}

// PARSE: text -> value, honouring value_type and the optional validator.
//
// Validator contract (design S3.3): when a validator is supplied it is handed a pointer to the
// matching scratch BindingValue member (not the final target), writes the parsed/validated
// value there, and returns its bool. The single store to the real target happens later in
// Binding_WriteSink. PRECONDITION: a supplied validator MUST write the BindingValue.as member
// selected by `type`; mismatched validator/type pairings are unsupported. A STRING validator is
// validate-only (it must not transform) - the non-owning source text is handed through unchanged.
bool Binding_ParseText(const char *text, BindingValueType type,
                       ValidatorFn validator, void *validator_ctx, BindingValue *out)
{
    if (!text || !out)
    {
        return false;
    }

    out->type = BIND_NONE; // no partial value escapes on failure

    switch (type)
    {
    case BIND_INT:
    {
        if (validator)
        {
            if (!validator(text, &out->as.i, validator_ctx)) return false;
        }
        else
        {
            char *end = NULL;
            long v = strtol(text, &end, 10);
            if (end == text) return false;
            out->as.i = (int)v;
        }
        out->type = BIND_INT;
        return true;
    }
    case BIND_FLOAT:
    {
        if (validator)
        {
            if (!validator(text, &out->as.f, validator_ctx)) return false;
        }
        else
        {
            char *end = NULL;
            float f = strtof(text, &end);
            if (end == text) return false;
            out->as.f = f;
        }
        out->type = BIND_FLOAT;
        return true;
    }
    case BIND_VECTOR2D:
    {
        if (validator)
        {
            if (!validator(text, &out->as.v, validator_ctx)) return false;
        }
        else
        {
            Vector2d parsed_vector = {0};
            if (!ParseVector2d(text, &parsed_vector)) return false;
            out->as.v = parsed_vector;
        }
        out->type = BIND_VECTOR2D;
        return true;
    }
    case BIND_STRING:
    {
        // STRING is non-owning: point the value at the caller's source text. A validator, if
        // present, is validate-only and receives the same non-owning text (no destination).
        if (validator)
        {
            if (!validator(text, (void *)text, validator_ctx)) return false;
        }
        out->as.s = text;
        out->type = BIND_STRING;
        return true;
    }
    case BIND_NONE:
    default:
        return false;
    }
}

// READ: resolve a source to a current BindingValue. NULL address/query degrade to BIND_NONE.
BindingValue Binding_ReadSource(const BindingSource *src)
{
    BindingValue value = {0};
    value.type = BIND_NONE;

    if (!src)
    {
        return value;
    }

    switch (src->kind)
    {
    case BIND_SRC_ADDRESS:
    {
        if (!src->address)
        {
            return value; // BIND_NONE
        }
        switch (src->value_type)
        {
        case BIND_INT:
            value.as.i = *(int *)src->address;
            value.type = BIND_INT;
            break;
        case BIND_FLOAT:
            value.as.f = *(float *)src->address;
            value.type = BIND_FLOAT;
            break;
        case BIND_VECTOR2D:
            value.as.v = *(Vector2d *)src->address;
            value.type = BIND_VECTOR2D;
            break;
        case BIND_STRING:
            value.as.s = (const char *)src->address;
            value.type = BIND_STRING;
            break;
        case BIND_NONE:
        default:
            break; // BIND_NONE
        }
        return value;
    }
    case BIND_SRC_QUERY:
    {
        if (!src->query)
        {
            return value; // BIND_NONE
        }
        value.as.i = src->query(src->query_key);
        value.type = BIND_INT;
        return value;
    }
    case BIND_SRC_NONE:
    default:
        return value; // BIND_NONE
    }
}

// WRITE: push a BindingValue to the sink. Returns false if the sink rejects.
bool Binding_WriteSink(const BindingSink *sink, BindingValue value)
{
    if (!sink)
    {
        return false;
    }

    switch (sink->kind)
    {
    case BIND_SINK_ADDRESS:
    {
        if (!sink->address)
        {
            return false;
        }
        switch (sink->value_type)
        {
        case BIND_INT:
            *(int *)sink->address = value.as.i;
            return true;
        case BIND_FLOAT:
            *(float *)sink->address = value.as.f;
            return true;
        case BIND_VECTOR2D:
            *(Vector2d *)sink->address = value.as.v;
            return true;
        case BIND_STRING:
        {
            // Bounded copy preserving the historic 255 cap with explicit NUL at index 255.
            if (!value.as.s)
            {
                return false;
            }
            strncpy((char *)sink->address, value.as.s, 255);
            ((char *)sink->address)[255] = '\0';
            return true;
        }
        case BIND_NONE:
        default:
            return false;
        }
    }
    case BIND_SINK_COMMAND:
    {
        // Dispatch only when a command fn is present and the code is not CMD_NONE (0).
        if (!sink->command || sink->command_code == 0)
        {
            return false;
        }
        sink->command(sink->command_code, NULL);
        return true;
    }
    case BIND_SINK_NONE:
    default:
        return false;
    }
}

// COMMIT: the full UI->data path. Forwards the sink's validator/ctx (passing NULL would
// silently drop validation for every validated binding).
bool Binding_Commit(const Binding *b, const char *text)
{
    if (!b || !text)
    {
        return false;
    }

    BindingValue v;
    if (!Binding_ParseText(text, b->sink.value_type,
                           b->sink.validator, b->sink.validator_ctx, &v))
    {
        return false; // caller reverts; target untouched
    }
    return Binding_WriteSink(&b->sink, v);
}

// REFRESH: the full data->UI path. Focused-skip is the caller's responsibility.
// A source that reads BIND_NONE leaves the output buffer untouched (last good display).
bool Binding_RefreshText(const Binding *b, char *out, size_t out_bytes)
{
    if (!b)
    {
        return false;
    }

    BindingValue v = Binding_ReadSource(&b->source);
    if (v.type == BIND_NONE)
    {
        return false;
    }
    return Binding_FormatValue(v, b->precision, out, out_bytes);
}

// Allocate a heap Binding and copy the caller's value into it. The Binding holds no nested
// heap pointers, so this is a flat copy; Binding_Destroy performs the matching shallow free.
Binding *Binding_Create(Binding value)
{
    Binding *b = (Binding *)AllocateBytes(sizeof(Binding));
    if (!b)
    {
        return NULL;
    }
    *b = value;
    return b;
}

// Shallow free of a heap Binding (holds no nested heap pointers) and NULL the caller pointer.
void Binding_Destroy(Binding **b)
{
    if (!b || !*b)
    {
        return;
    }
    Deallocate((void **)b, sizeof(Binding));
}

// Re-expressed as a thin adapter over the symmetric core (design S4.1). The validator writes
// into the scratch BindingValue and Binding_WriteSink stores to sink.address == b->target, so
// the final write lands at the identical address with identical bytes as the historic
// direct-to-target path, including the STRING 255-cap copy.
bool Binder_ValidateAndWrite(Binder *b, const char *text)
{
    if (!b || !text) return false;

    BindingSink sink = {
        .kind = BIND_SINK_ADDRESS,
        .value_type = b->type,
        .address = b->target,
        .validator = b->validator,
        .validator_ctx = b->user_data,
    };

    BindingValue v;
    if (!Binding_ParseText(text, b->type, b->validator, b->user_data, &v))
    {
        return false;
    }
    return Binding_WriteSink(&sink, v);
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
