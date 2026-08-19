#include "system/ui/utility_panel_system.h"

#include "system/panel_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"

static PanelSystem *utility_panel = NULL;
static Size utility_view_size = {{1.0f, 1.0f}, SIZE_PERCENT};

static void InitUtilityStatsContainer(void)
{
    UIElement *stats_container = CreateUIContainer(
        utility_panel->root, utility_view_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ui_standard_container_padding,
        utility_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

    const UIFieldSpec stats_specs[] = {
        {"Mem", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_mem_str},
        {"Fps", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_fps_str},
        {"Ftime", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_ftime_str},
    };
    InitUIFields(stats_container, stats_specs,
                 ARRAY_COUNT(stats_specs),
                 ui_standard_field_padding, utility_panel->palette);

    PanelSystem_CreateView(utility_panel, stats_container, 0);
}

void InitUtilityPanel(void)
{
    utility_panel = PanelSystem_Create(
        &utility_panel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
        &ui_default_palette, ui_standard_stack_spacing);
    if (!utility_panel)
    {
        return;
    }

    // Utility now contains telemetry only; debug controls live in lpanel STATE.
    PanelSystem_InitViews(utility_panel, 1);
    PanelSystem_InitRoot(utility_panel);
    InitUtilityStatsContainer();
    UpdateUISpace(utility_panel->root, utility_panel->seed_box);
}

void DrawUtilityPanel(void)
{
    if (utility_panel)
    {
        PanelSystem_Draw(utility_panel);
    }
}

UIElement *GetUtilityPanelRoot(void)
{
    return utility_panel ? utility_panel->root : NULL;
}
