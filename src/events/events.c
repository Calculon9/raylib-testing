#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "events/events.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)

static void InitScheduledTaskCommon(int interval, int run_limit, int *interval_frames, int *run_count, int *task_run_limit, bool *active)
{
    *interval_frames = interval;
    *run_count = 0; // Run immediately on first check, or set to 'interval' to delay it
    *task_run_limit = run_limit;
    *active = true;
}

static bool UpdateScheduledTaskCommon(int *run_count, int interval_frames, int run_limit, bool *active)
{
    (*run_count)++;

    // Has enough frames passed?
    if (*run_count % interval_frames == 0)
    {
        *active = *run_count < run_limit;
        return true;
    }

    return false;
}

ScheduledFunc CreateScheduledFunc(Func func, int interval, int run_limit)
{
    ScheduledFunc task;
    task.function = func;
    InitScheduledTaskCommon(interval,
                            run_limit,
                            &task.interval_frames,
                            &task.run_count,
                            &task.run_limit,
                            &task.active);
    return task;
}

ScheduledAction CreateScheduledAction(Action func, int interval, int run_limit)
{
    ScheduledAction task;
    task.function = func;
    InitScheduledTaskCommon(interval,
                            run_limit,
                            &task.interval_frames,
                            &task.run_count,
                            &task.run_limit,
                            &task.active);
    return task;
}

// This runs once per frame inside main game loop
void UpdateScheduledFunc(ScheduledFunc *task)
{
    if (task == NULL || !task->active || task->function == NULL || task->run_count >= task->run_limit)
        return;

    if (UpdateScheduledTaskCommon(&task->run_count, task->interval_frames, task->run_limit, &task->active))
    {
        task->function(task->data_context); // Run the passed function!
    }
}

// This runs once per frame inside main game loop
void UpdateScheduledAction(ScheduledAction *task)
{
    if (task == NULL || !task->active || task->function == NULL || task->run_count >= task->run_limit)
        return;

    if (UpdateScheduledTaskCommon(&task->run_count, task->interval_frames, task->run_limit, &task->active))
    {
        task->function(); // Run the passed function!
    }
}
