/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_INPUT_H
#define UI_INPUT_H
#include "common/common.h"
#include "system/systems.h"
#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

// #define FONT_DEFAULT FONT_BASIC

typedef struct
{
    String64 input_buffer;
    String64 output_buffer;
    String64 temp_buffer;
} Text_64_IOState;

typedef struct
{
    // Vector2d initial_coords; // Pixel coords of the initiating down-click
    Vector2d initial_pos;
    Vector2d current_pos;  // Updated every frame while the mouse is held down
    Vector2d previous_pos; // Updated every frame while the mouse is held down
    int left_button_hold_ticks;
    int right_button_hold_ticks;
} MouseDownState;

typedef struct
{
    UIElement *target_element;
    Vector2d drag_delta;
    Vector2d initial_element_offset; 
} DragState;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Coordinate Space Properties
// extern Vector2d lpanel_u;// = {1, 0};
// extern Vector2d lpanel_v;// = {0, 1};
// extern Vector2d lpanel_resolution; //= {0};
// extern Vector2d lpanel_pixel_origin, lpanel_pixel_end; //= {0};

// // Logical->pixel-space conversion properties
// extern Vector2d lpanel_pixel_u;// = {75, 0};
// extern Vector2d lpanel_pixel_v;// = {0, 75};
// extern Vector2d lpanel_origin, lpanel_end; //= {0}; // Dependent on the game world screen area
// extern Camera2d camera_lpanel;// = {0};

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);

#endif