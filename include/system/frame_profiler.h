// PROFILER - THROWAWAY per-phase frame profiler. To remove: delete this header and
// src/engine/system/frame_profiler.c, then delete every line/block tagged "// PROFILER".
#ifndef FRAME_PROFILER_H
#define FRAME_PROFILER_H

// PROFILER - Single compile-time switch. Set to 0 and every macro below expands to nothing.
#define FRAME_PROFILER_ENABLED 1

#if FRAME_PROFILER_ENABLED

// Phase identifiers. Each phase accumulates across multiple calls within one frame.
typedef enum
{
    PROF_FRAME = 0, // Whole frame wall time (includes the SetTargetFPS wait inside EndDrawing)
    PROF_UPDATE,    // Update half of the frame
    PROF_DRAW,      // Draw half, between BeginDrawing and EndDrawing
    PROF_ENDDRAW,   // EndDrawing() itself (buffer swap, vsync/driver wait, target-FPS sleep)
    PROF_CMDS,      // ProcessCommandQueue()
    PROF_RESET,     // collisions + resolved_collisions reset, CellOccupancy_Reset/Reserve
    PROF_PHYSICS,   // PhysicsUpdateJob via ExecuteJobs
    PROF_REMAP,     // Attachment/container remap loop
    PROF_SOLID,     // ResolveSolidCellCollisions
    PROF_PAIRS,     // Entity-pair broadphase loop
    PROF_GRIDFILL,  // DrawGridSpace fill pass (includes textured cells)
    PROF_GRIDLINES, // DrawGridSpace line pass
    PROF_GRIDLABELS,// DrawGridSpace debug label pass
    PROF_WORLDREGION, // Whole DrawWorldRegion call
    PROF_COUNT
} ProfilePhase;

// Start timing a phase (stores the start time; phases must not nest with themselves).
void FrameProfiler_Begin(ProfilePhase phase);
// Stop timing a phase and add the elapsed time to this frame's total for it.
void FrameProfiler_End(ProfilePhase phase);
// Call once at the very start of a frame: starts the whole-frame timer.
void FrameProfiler_FrameBegin(void);
// Call once after EndDrawing: folds the frame into the per-second aggregates and prints the report.
void FrameProfiler_FrameEnd(void);

// Record the grid cell size in pixels (printed as cellpx= on the first report line).
void FrameProfiler_SetCellPx(float cell_px);
#define PROFILE_BEGIN(id) FrameProfiler_Begin(id)
#define PROFILE_END(id) FrameProfiler_End(id)
#define PROFILE_FRAME_BEGIN() FrameProfiler_FrameBegin()
#define PROFILE_FRAME_END() FrameProfiler_FrameEnd()
#define PROFILE_SET_CELL_PX(px) FrameProfiler_SetCellPx(px)

#else

#define PROFILE_BEGIN(id) ((void)0)
#define PROFILE_END(id) ((void)0)
#define PROFILE_FRAME_BEGIN() ((void)0)
#define PROFILE_FRAME_END() ((void)0)
#define PROFILE_SET_CELL_PX(px) ((void)0)

#endif // FRAME_PROFILER_ENABLED

#endif // FRAME_PROFILER_H
