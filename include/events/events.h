/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef EVENTS_H
#define EVENTS_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_ARRAY(count, type) NewArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Function signature type
typedef void (*Action)(void);
typedef void (*Func)(void *data_context);

typedef struct
{
    Action function;     // The function to execute
    int interval_frames; // How often to run it (e.g., 60 for once per 60 frames)
    int run_count;       // Counts down or up to track the elapsed frames
    int run_limit;
    bool active; // Can toggle this schedule on or off
} ScheduledAction;

typedef struct
{
    Func function;       // The function to execute
    void *data_context;  // The payload / input argument data
    int interval_frames; // How often to run it (e.g., 60 for once per 60 frames)
    int run_count;       // Counts down or up to track the elapsed frames
    int run_limit;
    bool active; // Can toggle this schedule on or off
} ScheduledFunc;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
ScheduledFunc CreateScheduledFunc(Func func, int interval, int run_limit);
ScheduledAction CreateScheduledAction(Action func, int interval, int run_limit);
void UpdateScheduledFunc(ScheduledFunc *task);
void UpdateScheduledAction(ScheduledAction *task);

#endif
