#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include "system/utility_system.h"
#include "system/systems.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
//double memory_allocated = {0};

//float fps = {0};


void UpdateFrameCounter(FrameCounter *fc) {
    double current_time = GetPreciseTime(); // Or OS-specific high-res timer
    fc->delta_time = (float)(current_time - fc->last_time);
    fc->last_time = current_time;

    // Method 1: Instant FPS
    fc->fps = 1.0f / fc->delta_time;

    // Method 2: Averaged FPS (Updates once per second for readability)
    fc->timer += fc->delta_time;
    fc->frame_count_this_second++;

    if (fc->timer >= 1.0) {
        //printf("FPS: %d\n", fc->frame_count_this_second);
        fc->frame_count_this_second = 0;
        fc->timer = 0.0;
    }
    fc->total_frames++;
}

// Utilities (FPS) Initialization logic
void InitUtilities()
{
    InitFrameCounter();
    // framesCounter = 0;
    // finishScreen = 0;
}

FrameCounter InitFrameCounter()
{
    // TODO: Initialize GAMEPLAY screen variables here!
    FrameCounter fc = {0};
    //frameTimes = NEW_QUEUE(32, double); // Queue to hold timestamps of previous frames
    fc.last_time = GetPreciseTime();

    return fc;
}

// Utilities Update logic
void UpdateUtilities()
{
    UpdateFrameCounter(&frame_counter);
    GetCurrentMemoryAllocated();
    //UpdateFps();
}

// FPS Update logic
// void UpdateFps()
// {
//     // Need to remove the oldest frame time and add the current time to maintain a sliding window of frame times for accurate FPS calculation
//     prevFrameTime = currTime;
//     currTime = GetPreciseTime();

//     // 1. If the queue is full, we must remove the OLDEST to make room
//     double oldestTime;
//     bool queueWasFull = (frameTimes->coll->count == frameTimes->coll->capacity);

//     if (queueWasFull)
//     {
//         Queue_Pop(frameTimes, &oldestTime); // Remove the oldest time to maintain the sliding window
//     }

//     // 2. Add the NEWEST time
//     Queue_Push(frameTimes, &currTime);

//     // 3. Calculate FPS only if we have a full window of data
//     if (queueWasFull)
//     {
//         double timeSpan = currTime - oldestTime;

//         if (timeSpan > 0)
//         {
//             // Use capacity because that's the size of our window
//             fps.fps = (float)(frameTimes->coll->capacity / timeSpan);
//             // printf("FPS: %.2f\n", fps.fps); // Debug print to verify FPS calculation
//         }
//     }
//     else
//     {
//         fps.fps = 0.0f;
//     }
// }

double GetPreciseTime()
{
    struct timespec ts;
    // TIME_UTC is the standard base for "now"
    timespec_get(&ts, TIME_UTC);

    // Convert seconds and nanoseconds into a single double
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
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

Vector2d GetPointerTravelDelta(PointerState pointer_state)
{
    return VectorSum_2d(pointer_state.current_pos,
                        (Vector2d){-pointer_state.initial_pos.x, -pointer_state.initial_pos.y});
}

float GetPointerTravelMagnitude(PointerState pointer_state)
{
    return VectorMagnitude_2d(GetPointerTravelDelta(pointer_state));
}

bool IsPointerClick(PointerState pointer_state, int max_hold_ticks, float max_travel_pixels)
{
    if (pointer_state.left_button_hold_ticks <= 0 || pointer_state.left_button_hold_ticks >= max_hold_ticks)
    {
        return false;
    }

    return GetPointerTravelMagnitude(pointer_state) < max_travel_pixels;
}

bool IsPointerDrag(PointerState pointer_state, float min_travel_pixels)
{
    if (pointer_state.left_button_hold_ticks <= 0)
    {
        return false;
    }

    return GetPointerTravelMagnitude(pointer_state) > min_travel_pixels;
}

// Utility function to get current memory allocated in bytes
size_t GetCurrentMemoryAllocated()
{
    return CurrBytesAllocated();
}

// void Concatenate(char *dest, size_t dest_size, const char *src) {
//     // This finds the current end of the string and appends src
//     // while ensuring we never exceed dest_size.
//     snprintf(dest + strlen(dest), dest_size - strlen(dest), "%s", src);
// }