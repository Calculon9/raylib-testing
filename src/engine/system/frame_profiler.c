// PROFILER - THROWAWAY per-phase frame profiler (see include/system/frame_profiler.h).
#include "system/frame_profiler.h"

#if FRAME_PROFILER_ENABLED

#include <stdio.h>
#include "raylib.h"
#include "math/coordinate_space.h"
#include "world/universe.h"

// Per-phase state: start time, this frame's running total, and the one-second aggregates.
static double phase_start[PROF_COUNT];
static double frame_total[PROF_COUNT];
static double second_sum[PROF_COUNT];
static double second_max[PROF_COUNT];
static int second_frames = 0;
static double second_started = -1.0;
static float last_cell_px = 0.0f; // Most recent grid cell size in pixels (set by DrawGridSpace).

// Record the grid cell size in pixels for the next report line.
void FrameProfiler_SetCellPx(float cell_px)
{
    last_cell_px = cell_px;
}

// Short labels used in the report, indexed by ProfilePhase.
static const char *const phase_names[PROF_COUNT] = {
    "frame", "update", "draw", "enddraw", "cmds", "reset", "physics", "remap", "solid",
    "pairs", "gridfill", "gridlines", "gridlabels", "worldregion"};

// Start timing a phase.
void FrameProfiler_Begin(ProfilePhase phase)
{
    phase_start[phase] = GetTime();
}

// Stop timing a phase and accumulate the elapsed seconds into this frame's total.
void FrameProfiler_End(ProfilePhase phase)
{
    frame_total[phase] += GetTime() - phase_start[phase];
}

// Start the whole-frame timer.
void FrameProfiler_FrameBegin(void)
{
    if (second_started < 0.0)
    {
        second_started = GetTime();
    }
    FrameProfiler_Begin(PROF_FRAME);
}

// Fold the finished frame into the per-second aggregates; once a second, print the report and reset.
void FrameProfiler_FrameEnd(void)
{
    FrameProfiler_End(PROF_FRAME);

    // Fold this frame's totals into the sums and maxima, then clear them for the next frame.
    for (int p = 0; p < PROF_COUNT; p++)
    {
        second_sum[p] += frame_total[p];
        if (frame_total[p] > second_max[p])
        {
            second_max[p] = frame_total[p];
        }
        frame_total[p] = 0.0;
    }
    second_frames++;

    if (GetTime() - second_started < 1.0)
    {
        return;
    }

    // Count cells only here (once a second) so the profiler does not distort the measurement.
    long total_cells = 0;
    long present_cells = 0;
    for (int w = 0; w < G_Universe.world_count; w++)
    {
        const DArray *cells = &G_Universe.worlds[w].grid_space.space.cells;
        const Cell *items = (const Cell *)cells->items;
        total_cells += (long)cells->count;
        for (size_t c = 0; c < cells->count; c++)
        {
            if (items[c].flags & CELL_FLAG_PRESENT)
            {
                present_cells++;
            }
        }
    }

    // Line 1: totals and the frame/update/draw split. Line 2: per-phase avg/max in milliseconds.
    printf("[PROFILE] cells=%ld present=%ld fps=%d frames=%d cellpx=%.2f\n", total_cells, present_cells,
           GetFPS(), second_frames, last_cell_px);
    printf("[PROFILE] ms avg/max:");
    for (int p = 0; p < PROF_COUNT; p++)
    {
        printf(" %s=%.2f/%.2f%s", phase_names[p], 1000.0 * second_sum[p] / second_frames,
               1000.0 * second_max[p], (p == PROF_COUNT - 1) ? "" : " |");
    }
    printf("\n");
    fflush(stdout);

    // Reset the aggregates for the next second.
    for (int p = 0; p < PROF_COUNT; p++)
    {
        second_sum[p] = 0.0;
        second_max[p] = 0.0;
    }
    second_frames = 0;
    second_started = GetTime();
}

#endif // FRAME_PROFILER_ENABLED
