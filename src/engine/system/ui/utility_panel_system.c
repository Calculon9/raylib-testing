#include "system/ui/utility_panel_system.h"

#include "system/panel_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"

static PanelSystem *utility_panel = NULL;
static Size utility_view_size = {{1.0f, 1.0f}, SIZE_PERCENT};

static void InitUtilityStatsView(void)
{
    View *view = PanelSystem_CreateView(utility_panel, 0);
    UIElement *stats_container = view ? view->container : NULL;
    if (!stats_container)
    {
        return;
    }

    // Customise the standard view container for the utility statistics layout.
    stats_container->size = utility_view_size;
    stats_container->colour_border = utility_panel->palette->container_border;
    stats_container->colour_fill = utility_panel->palette->container_fill;
    stats_container->is_draggable = true;

    const UIFieldSpec stats_specs[] = {
        {"Mem", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_mem_str},
        {"Fps", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_fps_str},
        {"Ftime", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.stats_ftime_str},
    };
    InitUIFields(stats_container, stats_specs,
                 ARRAY_COUNT(stats_specs),
                 ui_standard_field_padding, utility_panel->palette);

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
    InitUtilityStatsView();
    UpdateUISpace(utility_panel->root, utility_panel->seed_box);
}

// Destroy the utility panel and clear its cached UI references.
void DestroyUtilityPanel(void)
{
    PanelSystem *panel = utility_panel;
    utility_panel = NULL;
    PanelSystem_Destroy(panel);

    G_UIState.stats_polygs_str = NULL;
    G_UIState.stats_fps_str = NULL;
    G_UIState.stats_ftime_str = NULL;
    G_UIState.stats_mem_str = NULL;
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

PanelSystem *GetUtilityPanelSystem(void)
{
    return utility_panel;
}
