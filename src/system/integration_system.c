/**********************************************************************************************
 *
 *  THIS MODULE INTEGRATES THE WORLD AND UI SYSTEMS THROUGH PIPELINEING & UTILITY FUNCTIONS
 *
 **********************************************************************************************/
#include "raylib.h"
#include "system/systems.h"
#include "ui/ui.h"
#include "physics/polygonoid.h"
#include "common/common.h"

// Text must be in the following format: "x,y", "(x,y)", "(magnitude)(x,y)" and assumes the text has already been committed by the user (e.g. pressed enter)
bool PipelineTextToVector(char *input_buffer, Vector2d *target_vector) //, NewtonProperty object_property)
{
    // FOR NOW: Only applies to TextBoxes
    // if (!element || !target_object || !IsTextbox(element))
    //     return;

    // if (element->is_focused)
    {
        float parsed_x = 0.0f;
        float parsed_y = 0.0f;
        float parsed_mag = 0.0f;
        bool valid_parse = false;

        // PIPELINE STEP 1: Parse and validate based on your two formatting rules
        // Format Style A: Check for parenthesized coordinate string "(x,y)"
        if (sscanf(input_buffer, "(%f,%f)", &parsed_x, &parsed_y) == 2)
        {
            valid_parse = true;
        }
        // Format Style B: Check for standard comma-separated string "x,y"
        else if (sscanf(input_buffer, "%f,%f", &parsed_x, &parsed_y) == 2)
        {
            valid_parse = true;
        }
        // Format Style B: Check for standard comma-separated string "x,y"
        else if (sscanf(input_buffer, "(%f)(%f,%f)", &parsed_mag, &parsed_x, &parsed_y) == 3)
        {
            valid_parse = true;
        }

        // PIPELINE STEP 2: If validation checks clear, pipe vectors out to physics structures
        if (valid_parse)
        {
            target_vector->x = parsed_x;
            target_vector->y = parsed_y;
            // switch (object_property)
            // {
            // case POSITION:
            //     target_object->newtonian_properties.coords_origin.x = parsed_x;
            //     target_object->newtonian_properties.coords_origin.y = parsed_y;
            //     break;
            // case VELOCITY:
            //     target_object->newtonian_properties.velocity.x = parsed_x;
            //     target_object->newtonian_properties.velocity.y = parsed_y;
            //     break;
            // case ACCELERATION:
            //     target_object->newtonian_properties.acceleration.x = parsed_x;
            //     target_object->newtonian_properties.acceleration.y = parsed_y;
            //     break;
            // case MOMENTUM:
            //     target_object->newtonian_properties.momentum.x = parsed_x;
            //     target_object->newtonian_properties.momentum.y = parsed_y;
            //     break;
            // default:
            //     break;
            // }
            return true;
        }
        else
        {
            return false;
            // The string did not conform to "x,y" or "(x,y)".
            // We consciously do nothing, leaving the current velocity unaffected.
        }
    }
    // else
    {
        // TELEMETRY STEP: The user is NOT interacting with this field.
        // Format the object's active vector back out to the UI display buffer array.
        // We output standard "(x,y)" format to look neat inside the statistics container.
        // snprintf(input_buffer, 64, "(%.2f,%.2f)", target_object->newtonian_properties.velocity.x, target_object->newtonian_properties.velocity.y);
    }
}
bool PipelineTextToInt(char *input_buffer, int *target_int)
{
    if (!input_buffer || !target_int)
        return false;

    int parsed_value = 0;
    if (sscanf(input_buffer, "%d", &parsed_value) == 1)
    {
        *target_int = parsed_value;
        return true;
    }

    return false;
}
// Text must be in the following format: "y" and assumes the text has already been committed by the user (e.g. pressed enter)
bool PipelineTextToFloat(char *input_buffer, float *target_float) //, NewtonProperty object_property)
{
    // FOR NOW: Only applies to TextBoxes
    // if (!element || !target_object || !IsTextbox(element))
    //     return;

    // if (element->is_focused)
    {
        float parsed_x = 0.0f;
        bool valid_parse = false;

        // PIPELINE STEP 1: Parse and validate based on your two formatting rules
        // Format Style A: Check for parenthesized coordinate string "(x,y)"
        if (sscanf(input_buffer, "%f", &parsed_x) == 1)
        {
            valid_parse = true;
        }

        // PIPELINE STEP 2: If validation checks clear, pipe vectors out to physics structures
        if (valid_parse)
        {
            *target_float = parsed_x;
            // switch (object_property)
            // {
            // case MASS:
            //     target_object->newtonian_properties.mass = parsed_x;
            //     break;
            // default:
            //     break;
            // }
            return true;
        }
        else
        {
            return false;
            // The string did not conform to "x,y" or "(x,y)".
            // We consciously do nothing, leaving the current velocity unaffected.
        }
    }
    // else
    {
        // TELEMETRY STEP: The user is NOT interacting with this field.
        // Format the object's active vector back out to the UI display buffer array.
        // We output standard "(x,y)" format to look neat inside the statistics container.
        // snprintf(input_buffer, 64, "(%.2f,%.2f)", target_object->newtonian_properties.velocity.x, target_object->newtonian_properties.velocity.y);
    }
}

// Writes vector components as "(magnitude)(x,y)"
void PipelineVectorToText(Vector2d input_vector, char *target_buffer, size_t target_buffer_bytes) //, NewtonProperty object_property)
{
    if (!target_buffer)
        return;

    snprintf(target_buffer, target_buffer_bytes, "(%.1f)(%.1f,%.1f)", VectorMagnitude_2d(input_vector), input_vector.x, input_vector.y);
    // switch (object_property)
    // {
    // case POSITION:
    // case VELOCITY:
    // case ACCELERATION:
    // case MOMENTUM:
    //     snprintf(target_buffer, target_buffer_bytes, "(%.1f)(%.1f,%.1f)", VectorMagnitude_2d(input_vector), input_vector.x, input_vector.y);
    //     break;
    // default:
    //     break;
    // }
}

// Writes vector components as "x.y"
void PipelineNumberToText(float input_float, int precision, char *target_buffer, size_t target_buffer_bytes) //, NewtonProperty object_property)
{
    if (!target_buffer)
        return;

    char format_spec[16];
    snprintf(format_spec, sizeof(format_spec), "%%.%df", precision);
    snprintf(target_buffer, target_buffer_bytes, format_spec, input_float);
}

// snprintf(G_UIState.lpanel_properties_mass_str->string, sizeof(String64), "%.1f", mass);
// snprintf(G_UIState.lpanel_properties_pos_str->string, sizeof(String64), "(%.1f)[%.1f,%.1f]", VectorMagnitude_2d(pos), pos.x, pos.y);