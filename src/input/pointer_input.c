#include "input/pointer_input.h"

Vector2d Pointer_GetTravelDelta(PointerState pointer_state)
{
    return VectorSum_2d(pointer_state.current_pos,
                        (Vector2d){-pointer_state.initial_pos.x, -pointer_state.initial_pos.y});
}

float Pointer_GetTravelMagnitude(PointerState pointer_state)
{
    return VectorMagnitude_2d(Pointer_GetTravelDelta(pointer_state));
}

bool Pointer_IsClick(PointerState pointer_state, int max_hold_ticks, float max_travel_pixels)
{
    if (pointer_state.left_button_hold_ticks <= 0 || pointer_state.left_button_hold_ticks >= max_hold_ticks)
    {
        return false;
    }

    return Pointer_GetTravelMagnitude(pointer_state) < max_travel_pixels;
}

bool Pointer_IsDrag(PointerState pointer_state, float min_travel_pixels)
{
    if (pointer_state.left_button_hold_ticks <= 0)
    {
        return false;
    }

    return Pointer_GetTravelMagnitude(pointer_state) > min_travel_pixels;
}

void UpdatePointerState(PointerButton button, PointerState *pointer_state, Vector2d pointer_pos)
{
    if (!pointer_state)
    {
        return;
    }

    switch (button)
    {
    case POINTER_BUTTON_LEFT:
        pointer_state->left_button_hold_ticks++;
        break;
    case POINTER_BUTTON_RIGHT:
        pointer_state->right_button_hold_ticks++;
        break;
    default:
        break;
    }

    if (pointer_state->left_button_hold_ticks == 1 || pointer_state->right_button_hold_ticks == 1)
    {
        pointer_state->initial_pos = pointer_pos;
        pointer_state->current_pos = pointer_pos;
        return;
    }

    pointer_state->previous_pos = pointer_state->current_pos;
    pointer_state->current_pos = pointer_pos;
}

void ResetPointerState(PointerState *pointer_state)
{
    if (!pointer_state)
    {
        return;
    }

    pointer_state->left_button_hold_ticks = 0;
    pointer_state->right_button_hold_ticks = 0;
    pointer_state->initial_pos = ZERO_VECTOR_2D;
    pointer_state->current_pos = ZERO_VECTOR_2D;
    pointer_state->previous_pos = ZERO_VECTOR_2D;
}

