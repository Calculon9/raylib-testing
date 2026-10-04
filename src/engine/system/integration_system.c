/**********************************************************************************************
 *
 *  THIS MODULE INTEGRATES THE WORLD AND UI SYSTEMS THROUGH PIPELINEING & UTILITY FUNCTIONS
 *
 **********************************************************************************************/
#include "raylib.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "ui/ui.h"
#include "physics/newtonoid.h"
#include "common/common.h"

void BindTextbox(UIElement *textbox, void *data_bind)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.data_bind = data_bind;
    if (!data_bind && textbox->data.textbox.binder)
    {
        Binder_Destroy(textbox->data.textbox.binder);
        textbox->data.textbox.binder = NULL;
    }
}

// Map the UI's field data type to the common text binding parser type.
static BindingType ResolveBindingType(DataType type)
{
    switch (type)
    {
    case INT:
        return BINDING_INT;
    case FLOAT:
        return BINDING_FLOAT;
    case VECTOR2D:
        return BINDING_VECTOR2D;
    case STRING64:
    case STRING128:
    case STRING256:
        return BINDING_STRING;
    default:
        return BINDING_NONE;
    }
}

void BindTextboxData(UIElement *textbox, DataType type, void *data_bind)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.data_type = type;
    BindingType binding_type = ResolveBindingType(type);
    if (!data_bind || binding_type == BINDING_NONE)
    {
        BindTextbox(textbox, NULL);
        return;
    }

    if (!textbox->data.textbox.binder)
    {
        textbox->data.textbox.binder = Binder_Create(binding_type, data_bind, NULL, NULL);
    }
    else
    {
        textbox->data.textbox.binder->type = binding_type;
        textbox->data.textbox.binder->target = data_bind;
        textbox->data.textbox.binder->validator = NULL;
        textbox->data.textbox.binder->user_data = NULL;
    }

    textbox->data.textbox.data_bind = data_bind;
}

// Build a STABLE (fixed-address) bidirectional binding: source and sink both target the same
// address, interpreted per the mapped BindingValueType. No panel function is involved - the
// address never moves. Validator-free by design (matches the create-field behaviour today).
// Co-located with BindTextboxData so it can reuse the static ResolveBindingType without changing
// its linkage. No-op on a NULL textbox or a DataType that resolves to BINDING_NONE.
void BindTextboxStable(UIElement *textbox, DataType type, void *address, int precision)
{
    if (!textbox)
    {
        return;
    }

    BindingValueType value_type = ResolveBindingType(type);
    if (value_type == BINDING_NONE)
    {
        return;
    }

    Binding built = {
        .source = {
            .kind = BIND_SRC_ADDRESS,
            .value_type = value_type,
            .address = address,
        },
        .sink = {
            .kind = BIND_SINK_ADDRESS,
            .value_type = value_type,
            .address = address,
        },
        .precision = precision,
    };

    // Overwrite an existing binding in place to avoid leaking a live pointer; only allocate when
    // the slot is empty (mirrors BindTextboxData's binder reuse).
    if (textbox->data.textbox.binding)
    {
        *textbox->data.textbox.binding = built;
    }
    else
    {
        textbox->data.textbox.binding = Binding_Create(built);
    }
}

// Build a DYNAMIC binding: a panel-supplied query drives the display read and a panel-supplied
// callback stores the committed value. The panel owns all selection / clear-when-none policy;
// the core only calls the function pointers keyed on the opaque int. No-op on a NULL textbox.
void BindTextboxDynamic(UIElement *textbox, BindingValueType type, int precision,
                        BindingQueryFn query, BindingSinkFn write, int key)
{
    if (!textbox)
    {
        return;
    }

    Binding built = {
        .source = {
            .kind = BIND_SRC_QUERY,
            .value_type = type,
            .query = query,
            .query_key = key,
        },
        .sink = {
            .kind = BIND_SINK_CALLBACK,
            .value_type = type,
            .write = write,
            .write_key = key,
        },
        .precision = precision,
    };

    // Same create-or-overwrite-in-place rule as BindTextboxStable (no leak over a live pointer).
    if (textbox->data.textbox.binding)
    {
        *textbox->data.textbox.binding = built;
    }
    else
    {
        textbox->data.textbox.binding = Binding_Create(built);
    }
}

// Detach and free a textbox's binding, NULLing the slot. No-op on a NULL textbox or a textbox
// that carries no binding. Provided for completeness; neither live proof needs detach.
void ClearTextboxBinding(UIElement *textbox)
{
    if (!textbox || !textbox->data.textbox.binding)
    {
        return;
    }

    Binding_Destroy(&textbox->data.textbox.binding);
}

void BindTextboxGroup(UIElement **textboxes, void **bindings, size_t count)
{
    if (!textboxes || !bindings)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        BindTextbox(textboxes[i], bindings[i]);
    }
}

void ClearTextbox(UIElement *textbox)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.text.string[0] = '\0';
}

void ClearAndUnbindTextbox(UIElement *textbox)
{
    ClearTextbox(textbox);
    BindTextbox(textbox, NULL);
}

void ClearAndUnbindTextboxGroup(UIElement **textboxes, size_t count)
{
    if (!textboxes)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        ClearAndUnbindTextbox(textboxes[i]);
    }
}

void WriteTextboxText(UIElement *textbox, const char *value)
{
    if (!textbox || !value)
    {
        return;
    }

    safe_strncpy(textbox->data.textbox.text.string, value, MAX_LABEL_CHARS);
}

void WriteTextboxInt(UIElement *textbox, int value)
{
    if (!textbox)
    {
        return;
    }

    UpdateString64(textbox->data.textbox.text.string, "%d", value);
}

void WriteTextboxFloat(UIElement *textbox, float value, int precision)
{
    if (!textbox)
    {
        return;
    }

    PipelineNumberToText(value, precision, textbox->data.textbox.text.string, sizeof(String64));
}

void WriteTextboxVector(Vector2d value, UIElement *textbox)
{
    if (!textbox)
    {
        return;
    }

    PipelineVectorToText(value, textbox->data.textbox.text.string, sizeof(String64));
}

void WriteTextboxVectorPair(UIElement *textbox, Vector2d value)
{
    if (!textbox)
    {
        return;
    }

    UpdateString64(textbox->data.textbox.text.string, "(%.2f,%.2f)", value.x, value.y);
}

void WriteTextboxNumberIfUnfocused(UIElement *textbox, float value, int precision)
{
    if (!textbox || textbox->is_focused)
    {
        return;
    }

    WriteTextboxFloat(textbox, value, precision);
}

void WriteTextboxVectorIfUnfocused(UIElement *textbox, Vector2d value)
{
    if (!textbox || textbox->is_focused)
    {
        return;
    }

    WriteTextboxVector(value, textbox);
}

void RefreshTextboxFields(const TextboxField *fields, size_t count)
{
    if (!fields)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        const TextboxField *field = &fields[i];
        if (!field->textbox)
        {
            continue;
        }

        if (!field->data_bind)
        {
            if (field->empty_text)
            {
                WriteTextboxText(field->textbox, field->empty_text);
                BindTextbox(field->textbox, NULL);
            }
            else
            {
                ClearAndUnbindTextbox(field->textbox);
            }
            continue;
        }

        BindTextboxData(field->textbox, field->data_type, field->data_bind);
        // Preserve the focused-skip invariant: never clobber text while the user is typing.
        if (field->textbox->is_focused)
        {
            continue;
        }

        // Route the data->UI read through the symmetric binding core. The source is the bound
        // address interpreted per the field's data type; the sink is read-only (none). Output
        // is byte-identical to the previous switch (INT "%d", FLOAT "%.<precision>f",
        // VECTOR2D "(%.2f,%.2f)"); STRING data types format nothing, matching the old default.
        Binding binding = {
            .source = {
                .kind = BIND_SRC_ADDRESS,
                .value_type = ResolveBindingType(field->data_type),
                .address = field->data_bind,
            },
            .sink = { .kind = BIND_SINK_NONE },
            .precision = field->precision,
        };

        // STRING sources are not formatted today (the old switch had no STRING case); skip them
        // so a stale/last-good display is preserved exactly as before.
        if (binding.source.value_type == BIND_STRING)
        {
            continue;
        }

        Binding_RefreshText(&binding, field->textbox->data.textbox.text.string, sizeof(String64));
    }
}

// Writes vector components as "(x,y)"
void PipelineVectorToText(Vector2d input_vector, char *target_buffer, size_t target_buffer_bytes)
{
    if (!target_buffer)
        return;

    snprintf(target_buffer, target_buffer_bytes, "(%.2f,%.2f)", input_vector.x, input_vector.y);
}

// Writes vector components as "x.y"
void PipelineNumberToText(float input_float, int precision, char *target_buffer, size_t target_buffer_bytes)
{
    if (!target_buffer)
        return;

    char format_spec[16];
    snprintf(format_spec, sizeof(format_spec), "%%.%df", precision);
    snprintf(target_buffer, target_buffer_bytes, format_spec, input_float);
}
