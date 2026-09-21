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
        if (field->textbox->is_focused)
        {
            continue;
        }

        switch (field->data_type)
        {
        case FLOAT:
            WriteTextboxFloat(field->textbox, *(float *)field->data_bind, field->precision);
            break;
        case INT:
            WriteTextboxInt(field->textbox, *(int *)field->data_bind);
            break;
        case VECTOR2D:
            WriteTextboxVector(*(Vector2d *)field->data_bind, field->textbox);
            break;
        default:
            break;
        }
    }
}

// // Text must be in the following format: "x,y", "(x,y)", "(magnitude)(x,y)".
// bool PipelineTextToVector(char *input_buffer, Vector2d *target_vector)
// {
//     if (!input_buffer || !target_vector)
//     {
//         return false;
//     }

//     float parsed_x = 0.0f;
//     float parsed_y = 0.0f;
//     float parsed_mag = 0.0f;
//     bool valid_parse =
//         (sscanf(input_buffer, "(%f,%f)", &parsed_x, &parsed_y) == 2) ||
//         (sscanf(input_buffer, "%f,%f", &parsed_x, &parsed_y) == 2) ||
//         (sscanf(input_buffer, "(%f)(%f,%f)", &parsed_mag, &parsed_x, &parsed_y) == 3);

//     if (!valid_parse)
//     {
//         return false;
//     }

//     target_vector->x = parsed_x;
//     target_vector->y = parsed_y;
//     return true;
// }
// bool PipelineTextToInt(char *input_buffer, int *target_int)
// {
//     if (!input_buffer || !target_int)
//         return false;

//     int parsed_value = 0;
//     if (sscanf(input_buffer, "%d", &parsed_value) == 1)
//     {
//         *target_int = parsed_value;
//         return true;
//     }

//     return false;
// }
// // Text must be in the following format: "y".
// bool PipelineTextToFloat(char *input_buffer, float *target_float)
// {
//     if (!input_buffer || !target_float)
//     {
//         return false;
//     }

//     float parsed_x = 0.0f;
//     if (sscanf(input_buffer, "%f", &parsed_x) != 1)
//     {
//         return false;
//     }

//     *target_float = parsed_x;
//     return true;
// }

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
