/**********************************************************************************************
*
UTILITY MODULE
*
**********************************************************************************************/
#ifndef UTILITY_SYSTEM_H
#define UTILITY_SYSTEM_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct {
//     float fps;
// } FPS;

typedef struct {
    time_t time;
} Frame;

typedef struct {
    float fps;
    float delta_time;
    double last_time;
    int frame_count_this_second;
    uint64_t total_frames;
    double timer;
} FrameCounter;

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

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
//extern float memory_allocated;


//extern FrameCounter frame_counter;
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void InitUtilities();
// void UpdateUtilities();
// void UpdateFps();
// void InitFps();
// //void CalculateFps();
// //float GetFps();
double GetPreciseTime();
size_t GetCurrentMemoryAllocated();
void UpdateFrameCounter(FrameCounter *fc);
void UpdatePointerState(PointerButton button, PointerState *pointer_state, Vector2d pointer_pos);
void ResetPointerState(PointerState *pointer_state);
Vector2d GetPointerTravelDelta(PointerState pointer_state);
float GetPointerTravelMagnitude(PointerState pointer_state);
bool IsPointerClick(PointerState pointer_state, int max_hold_ticks, float max_travel_pixels);
bool IsPointerDrag(PointerState pointer_state, float min_travel_pixels);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif