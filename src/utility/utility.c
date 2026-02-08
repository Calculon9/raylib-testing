#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "utility/utility.h"
#include "collections/queue.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
static Queue frame_times = {0};
static FPS fps = {0};
static time_t curr_time = {0};
//static time_t start_time = NULL;
//static const int time_interval = 1; // Time interval in seconds for FPS calculation
//static int frame_count = 0;


FPS GetFps()
{
    return fps;
}

// Utilities (FPS) Initialization logic
void InitUtilities()
{
    InitFps();
    // framesCounter = 0;
    // finishScreen = 0;
}

void InitFps()
{
    // TODO: Initialize GAMEPLAY screen variables here!
    frame_times = NEW_QUEUE(64, double); // Queue to hold timestamps of previous frames
}

// Utilities Update logic
void UpdateUtilities()
{
    UpdateFps();
}

// FPS Update logic
void UpdateFps()
{
    // Need to remove the oldest frame time and add the current time to maintain a sliding window of frame times for accurate FPS calculation

    double curr_time = GetPreciseTime();

    // 1. If the queue is full, we must remove the OLDEST to make room
    double oldest_time;
    bool queue_was_full = (frame_times.count == frame_times.capacity);

    if (queue_was_full) {
        pop(&frame_times, &oldest_time);
    }

    // 2. Add the NEWEST time
    push(&frame_times, &curr_time);

    // 3. Calculate FPS only if we have a full window of data
    if (queue_was_full) {
        double time_span = curr_time - oldest_time;
        
        if (time_span > 0) {
            // Use capacity because that's the size of our window
            fps.fps = (float)(frame_times.capacity / time_span);
            //printf("FPS: %.2f\n", fps.fps); // Debug print to verify FPS calculation
        }
    } else {
        fps.fps = 0.0f; 
    }
}

double GetPreciseTime() {
    struct timespec ts;
    // TIME_UTC is the standard base for "now"
    timespec_get(&ts, TIME_UTC);
    
    // Convert seconds and nanoseconds into a single double
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

// Utility function to get current memory allocated in bytes
float GetCurrentMemoryAllocated() {
    return curr_bytes_allocated();
}  