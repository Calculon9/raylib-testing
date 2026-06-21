#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "events/events.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)

ScheduledFunc CreateScheduledFunc(Func func, int interval, int run_limit)
{
    ScheduledFunc task;
    task.function = func;
    task.interval_frames = interval;
    task.run_count = 0; // Run immediately on first check, or set to 'interval' to delay it
    task.active = true;
    task.run_limit = run_limit;
    return task;
}

ScheduledAction CreateScheduledAction(Action func, int interval, int run_limit)
{
    ScheduledAction task;
    task.function = func;
    task.interval_frames = interval;
    task.run_count = 0; // Run immediately on first check, or set to 'interval' to delay it
    task.active = true;
    task.run_limit = run_limit;
    return task;
}

// This runs once per frame inside main game loop
void UpdateScheduledFunc(ScheduledFunc *task)
{
    if (task == NULL || !task->active || task->function == NULL || task->run_count >= task->run_limit)
        return;

    task->run_count++;

    // Has enough frames passed?
    if (task->run_count % task->interval_frames == 0)
    {
        task->function(task->data_context); // Run the passed function!
        task->active = task->run_count < task->run_limit;
    }
}

// This runs once per frame inside main game loop
void UpdateScheduledAction(ScheduledAction *task)
{
    if (task == NULL || !task->active || task->function == NULL || task->run_count >= task->run_limit)
        return;

    task->run_count++;

    // Has enough frames passed?
    if (task->run_count % task->interval_frames == 0)
    {
        task->function(); // Run the passed function!
        task->active = task->run_count < task->run_limit;
    }
}