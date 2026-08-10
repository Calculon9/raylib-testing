/**********************************************************************************************
*
POINTER INPUT MODULE
*
**********************************************************************************************/
#ifndef POINTER_INPUT_H
#define POINTER_INPUT_H

#include <stdbool.h>
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
    POINTER_BUTTON_LEFT,
    POINTER_BUTTON_RIGHT
} PointerButton;

typedef struct
{
    Vector2d initial_pos;
    Vector2d current_pos;
    Vector2d previous_pos;
    int left_button_hold_ticks;
    int right_button_hold_ticks;
} PointerState;

typedef struct
{
    Vector2d pointer_position;
    bool left_pressed;
    bool left_down;
    bool left_released;
    bool right_pressed;
    bool right_down;
    bool right_released;
    float wheel_delta;
} InputFrame;

void UpdatePointerState(PointerButton button, PointerState *pointer_state, Vector2d pointer_pos);
void ResetPointerState(PointerState *pointer_state);
Vector2d Pointer_GetTravelDelta(PointerState pointer_state);
float Pointer_GetTravelMagnitude(PointerState pointer_state);
bool Pointer_IsClick(PointerState pointer_state, int max_hold_ticks, float max_travel_pixels);
bool Pointer_IsDrag(PointerState pointer_state, float min_travel_pixels);

#endif
