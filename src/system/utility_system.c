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

double GetPreciseTime()
{
    struct timespec ts;
    // TIME_UTC is the standard base for "now"
    timespec_get(&ts, TIME_UTC);

    // Convert seconds and nanoseconds into a single double
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
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
