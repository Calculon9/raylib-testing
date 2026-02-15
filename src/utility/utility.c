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
static Queue *frameTimes = NULL;
static double prevFrameTime = {0};
static double currTime = {0};
static FPS fps = {0};
// static time_t start_time = NULL;
// static const int time_interval = 1; // Time interval in seconds for FPS calculation
// static int frame_count = 0;

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
    frameTimes = NEW_QUEUE(32, double); // Queue to hold timestamps of previous frames
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
    prevFrameTime = currTime;
    currTime = GetPreciseTime();

    // 1. If the queue is full, we must remove the OLDEST to make room
    double oldestTime;
    bool queueWasFull = (frameTimes->coll->count == frameTimes->coll->capacity);

    if (queueWasFull)
    {
        Queue_Pop(frameTimes, &oldestTime); // Remove the oldest time to maintain the sliding window
    }

    // 2. Add the NEWEST time
    Queue_Push(frameTimes, &currTime);

    // 3. Calculate FPS only if we have a full window of data
    if (queueWasFull)
    {
        double timeSpan = currTime - oldestTime;

        if (timeSpan > 0)
        {
            // Use capacity because that's the size of our window
            fps.fps = (float)(frameTimes->coll->capacity / timeSpan);
            // printf("FPS: %.2f\n", fps.fps); // Debug print to verify FPS calculation
        }
    }
    else
    {
        fps.fps = 0.0f;
    }
}

double GetPreciseTime()
{
    struct timespec ts;
    // TIME_UTC is the standard base for "now"
    timespec_get(&ts, TIME_UTC);

    // Convert seconds and nanoseconds into a single double
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

float GetFrameDeltaTime()
{
    // This function can be used to get the time taken for the last frame, which can be useful for physics calculations
    // For simplicity, we can return a fixed value or calculate it based on the frame times in the queue
    if (frameTimes->coll->count > 1)
    {
        return (float)(currTime - prevFrameTime);
    }
    else
    {
        return 0.016f; // Approximate frame time for 60 FPS
    }
}

// Utility function to get current memory allocated in bytes
size_t GetCurrentMemoryAllocated()
{
    return CurrBytesAllocated();
}