/**********************************************************************************************
*
UTILITY MODULE
*
**********************************************************************************************/
#ifndef UTILITY_SYSTEM_H
#define UTILITY_SYSTEM_H
#include <stddef.h>

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
// double GetPreciseTime();
// float GetFrameDeltaTime();
// size_t GetCurrentMemoryAllocated();
// void UpdateFrameCounter(FrameCounter *fc);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif