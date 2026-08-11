#include "system/ui/utility_panel_system.h"

#include "system/panel_system.h"
#include "system/debug_overlay_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"

static PanelSystem *utility_panel = NULL;
static Size utility_toggle_size = {{1.0f, 0.15f}, SIZE_PERCENT};
static Size utility_view_size = {{1.0f, 0.85f}, SIZE_PERCENT};
static ViewSelector *utility_view_selector = NULL;

typedef struct
{
    DebugOverlayId id;
    const char *label;
} UtilityDebugToggle;

static const UtilityDebugToggle utility_debug_toggles[] = {
    {DEBUG_WORLD_GRID, "World Grid"},
    {DEBUG_WORLD_GRID_LABELS, "Grid Text (World/F2)"},
    {DEBUG_UNIVERSE_GRID_LABELS, "Grid Text (Universe/F4)"},
};

static void HandleUtilityDebugToggleClick(UIElement *button)
{
    if (!button || !button->data.button.user_data)
    {
        return;
    }

    const UtilityDebugToggle *toggle = (const UtilityDebugToggle *)button->data.button.user_data;
    ToggleDebug(toggle->id);
    UpdateString64(button->data.button.label.string, "%s: %s", toggle->label,
                   IsDebugEnabled(toggle->id) ? "ON" : "OFF");
}

static void InitUtilityToggleButtons(void)
{
    UIElement *toggle_container = CreateUIContainer(
        utility_panel->root, utility_toggle_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        utility_panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_standard_inline_spacing, false, true);
    toggle_container->colour_border = utility_panel->palette->container_border;

    const char *labels[] = {"STATS", "DEBUG"};
    utility_view_selector = PanelSystem_CreateViewSelector(
        utility_panel, toggle_container, ui_standard_selector_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), NULL);
}

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
                 sizeof(stats_specs) / sizeof(stats_specs[0]),
                 ui_standard_field_padding, utility_panel->palette);

    View *stats_view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(utility_panel, stats_view, stats_container, 0);
}

static void InitUtilityDebugContainer(void)
{
    UIElement *debug_container = CreateUIContainer(
        utility_panel->root, utility_view_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ui_standard_container_padding,
        utility_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

    View *debug_view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(utility_panel, debug_view, debug_container, 1);

    for (size_t i = 0; i < (sizeof(utility_debug_toggles) / sizeof(utility_debug_toggles[0])); i++)
    {
        String64 label = {0};
        UpdateString64(label.string, "%s: %s", utility_debug_toggles[i].label,
                       IsDebugEnabled(utility_debug_toggles[i].id) ? "ON" : "OFF");

        CreateUIButtonDefault(
            debug_container, UI_ELEMENT_BUTTON_SIMPLE,
            label.string,
            ui_standard_button_size, ui_standard_button_padding, utility_panel->palette,
            HandleUtilityDebugToggleClick, (void *)&utility_debug_toggles[i], NULL);
    }
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

    PanelSystem_InitViews(utility_panel, 2);
    PanelSystem_InitRoot(utility_panel);
    InitUtilityToggleButtons();
    InitUtilityStatsContainer();
    InitUtilityDebugContainer();
    PanelSystem_SelectView(utility_view_selector, 0);
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
