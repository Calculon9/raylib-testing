/**********************************************************************************************
*
UTILITY PANEL SYSTEM MODULE (implementation)
*
Loads utility_panel.xml (the Performance card) and mounts it on the utility viewport through the
shared panel builder. The three read-only fields (Mem / Fps / Ftime) are bound to BIND_STRING query
shims, so the panel owns no C-built elements; each shim reformats its value every
UTILITY_STATS_REFRESH_FRAMES frames and returns the cached text in between.
*
**********************************************************************************************/
#include "system/ui/utility_panel_system.h"

#include <stdio.h>
#include <string.h>

#include "system/ui/panel_shared.h"
#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"

// The telemetry text is reformatted once per this many frames, to avoid per-frame formatting churn.
#define UTILITY_STATS_REFRESH_FRAMES 20

static ViewHostSystem *utility_panel = NULL;

// Panel-owned backing buffers for the BIND_STRING results (the pointer must outlive the refresh
// call, exactly like the info strip's label buffers). An empty buffer means "not formatted yet".
static char utility_mem_text[32];
static char utility_fps_text[32];
static char utility_ftime_text[32];

// True on frames where the telemetry text should be reformatted: every UTILITY_STATS_REFRESH_FRAMES
// frames, or whenever the buffer has never been filled (so a fresh panel never shows a blank value).
static bool Utility_ShouldRefresh(const char *buffer)
{
    return buffer[0] == '\0' || frame_counter.total_frames % UTILITY_STATS_REFRESH_FRAMES == 0;
}

// Memory in KiB currently allocated through the engine allocator ("%.1f").
static BindingValue Utility_QueryMem(int key)
{
    (void)key;
    if (Utility_ShouldRefresh(utility_mem_text))
    {
        snprintf(utility_mem_text, sizeof(utility_mem_text), "%.1f", (double)(GetCurrentMemoryAllocated() / 1024.0f));
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = utility_mem_text };
}

// Frames per second ("%.1f").
static BindingValue Utility_QueryFps(int key)
{
    (void)key;
    if (Utility_ShouldRefresh(utility_fps_text))
    {
        snprintf(utility_fps_text, sizeof(utility_fps_text), "%.1f", (double)frame_counter.fps);
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = utility_fps_text };
}

// Frame time in milliseconds ("%.2f").
static BindingValue Utility_QueryFtime(int key)
{
    (void)key;
    if (Utility_ShouldRefresh(utility_ftime_text))
    {
        snprintf(utility_ftime_text, sizeof(utility_ftime_text), "%.2f", (double)(frame_counter.delta_time * 1000));
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = utility_ftime_text };
}

// Resolve the panel's binding names ("utility.mem", "utility.fps", "utility.ftime") to the query
// shims above; anything else is unresolved.
static UIBinding Utility_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;
    static const struct { const char *name; BindingQueryFn query; } sources[] = {
        {"utility.mem", Utility_QueryMem},
        {"utility.fps", Utility_QueryFps},
        {"utility.ftime", Utility_QueryFtime},
    };
    if (binding_string)
    {
        for (size_t i = 0; i < ARRAY_COUNT(sources); i++)
        {
            if (!strcmp(binding_string, sources[i].name))
            {
                return (UIBinding){
                    .kind = UI_BIND_SRC_QUERY,
                    .query = sources[i].query,
                    .query_key = 0,
                    .value_type = BIND_STRING,
                };
            }
        }
    }
    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// The panel has no command buttons; every action name resolves to none.
static int Utility_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)cmd_string;
    (void)ctx;
    return 0;
}

// Build the Performance panel: load utility_panel.xml and mount it on the utility viewport through
// the shared panel builder (no tab selector), then lay it out over the valid viewport space.
void InitUtilityPanel(void)
{
    const PanelMarkupDesc desc = {
        .file = UI_COMPONENTS_DIR "/utility_panel.xml",
        .viewport = &utility_panel_viewport,
        .host_padding = {5.0f, 5.0f},
        .resolve_binding = Utility_ResolveBinding,
        .resolve_command = Utility_ResolveCommand,
    };
    ViewSelector *selector = NULL; // The markup declares no tabs, so the builder leaves this NULL.
    UIElement *root = NULL;
    utility_panel = PanelShared_BuildFromMarkup(&desc, &selector, &root);
    if (!utility_panel)
    {
        return;
    }
    UpdateUISpace(utility_panel->root, utility_panel->seed_box);
}

// Destroy the utility panel and clear its cached text.
void DestroyUtilityPanel(void)
{
    ViewHostSystem *panel = utility_panel;
    utility_panel = NULL;
    ViewHostSystem_Destroy(panel);

    utility_mem_text[0] = '\0';
    utility_fps_text[0] = '\0';
    utility_ftime_text[0] = '\0';
}

void DrawUtilityPanel(void)
{
    if (utility_panel)
    {
        ViewHostSystem_Draw(utility_panel);
    }
}

UIElement *GetUtilityPanelRoot(void)
{
    return utility_panel ? utility_panel->root : NULL;
}

ViewHostSystem *GetUtilityPanelViewHost(void)
{
    return utility_panel;
}
