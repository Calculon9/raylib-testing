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
#include <stdarg.h>
#include <stdio.h>

#include "common/common.h"

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
    float delta_time; // Time elapsed since last frame in milliseconds
    double last_time;
    int frame_count_this_second;
    uint64_t total_frames;
    double timer;
} FrameCounter;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
//extern float memory_allocated;


//extern FrameCounter frame_counter;
//----------------------------------------------------------------------------------
// String Helper Functions
//----------------------------------------------------------------------------------
// Copy up to dst_size-1 bytes and always NUL-terminate dst (if dst_size>0)
static inline void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if (!dst || !src || dst_size == 0)
        return;
    // Use snprintf behaviour to guarantee termination
    size_t i = 0;
    for (; i + 1 < dst_size && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

// Format and write to a String64 buffer with automatic null termination
// Returns number of characters written (excluding null terminator), or negative on error
static inline int UpdateString64(char *dest, const char *fmt, ...)
{
    if (!dest || !fmt)
        return -1;
    
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(dest, 64, fmt, args);
    va_end(args);
    
    return result;
}

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateFps();
// void InitFps();
// //void CalculateFps();
// //float GetFps();
double GetPreciseTime();
size_t GetCurrentMemoryAllocated();
void UpdateFrameCounter(FrameCounter *fc);
//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif
