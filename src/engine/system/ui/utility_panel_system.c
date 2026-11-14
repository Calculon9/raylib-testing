#include "system/ui/utility_panel_system.h"

#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"

static ViewHostSystem *utility_panel = NULL;
// The sheet fills the whole utility panel viewport (the card inside hugs its content).
static Size utility_view_size = {{1.0f, 1.0f}, AXIS_PERCENT, AXIS_PERCENT};
// The card spans the sheet's inner width and hugs its title and rows (a 0 hug dimension means uncapped).
static Size utility_card_size = {{1.0f, 0.0f}, AXIS_FILL, AXIS_HUG};
// Build the telemetry sheet: a CARD-role section (standard 6 px padding, cream title and labels,
// the standard 118 px label column) holding the Mem / Fps / Ftime read-only rows. This mirrors what
// the markup loader does for a surface="card" container with field-layout="stacked" TextFields.
static void InitUtilityStatsView(void)
{
    View *view = ViewHostSystem_CreateView(utility_panel, 0);
    UIElement *stats_container = view ? view->container : NULL;
    if (!stats_container)
    {
        return;
    }
    const UIPalette *palette = utility_panel->palette;
    // Apply the standard sheet styling (SHEET fill, no border, 6 px padding), as the markup panels get
    ViewHostSystem_StyleViewSheet(stats_container, palette);
    // Customise the standard view container size for the utility statistics layout.
    stats_container->size = utility_view_size;
    // CARD section: the same surface role, padding and stacking the loader gives a card container.
    UIElement *card = CreateUIContainer(stats_container, utility_card_size,
                                        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, ui_section_padding,
                                        palette, UI_PALETTE_SURFACE_CARD, ui_standard_stack_spacing,
                                        true, true);
    if (!card)
    {
        return;
    }
    // Section title, matching the other panels' titles (upper-cased by the title constructor), with
    // the card's cream text colour like a markup title inside a card.
    ColourRgba card_text = palette->title_text;
    const UISurfaceStyle card_style = UIPalette_GetSurfaceStyle(palette, UI_PALETTE_SURFACE_CARD);
    if (card_style.has_text)
    {
        card_text = card_style.text;
    }
    UIElement *title = CreateUILabelTitleDefault(card, "Performance", ui_title_size,
                                                 ui_standard_field_padding, palette);
    if (title)
    {
        title->data.label.font.colour = card_text;
    }
    UIElement *stats_boxes[3] = {NULL, NULL, NULL};
    const UIFieldSpec stats_specs[] = {
        {"Mem", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &stats_boxes[0], &G_UIState.stats_mem_str},
        {"Fps", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &stats_boxes[1], &G_UIState.stats_fps_str},
        {"Ftime", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &stats_boxes[2], &G_UIState.stats_ftime_str},
    };
    InitUIFields(card, stats_specs,
                 ARRAY_COUNT(stats_specs),
                 ui_standard_field_padding, palette);
    // Same as field-layout="stacked": flag each input so the per-section longest-label pass leaves the
    // fixed label column (ui_standard_textfield_label_width) alone, and give the labels the card text.
    for (size_t i = 0; i < ARRAY_COUNT(stats_boxes); i++)
    {
        UIElement *input = stats_boxes[i];
        if (!input)
        {
            continue;
        }
        input->data.textbox.stacked_field = true;
        UIElement *label = input->parent ? input->parent->first_child : NULL;
        if (label && label->type == UI_ELEMENT_LABEL)
        {
            label->data.label.font.colour = card_text;
        }
    }
    // Descendants inside a card resolve INACTIVE roles as the sheet colours (the loader stamps this for markup cards).
    UIElement_MarkDescendantsInCard(card);
}
void InitUtilityPanel(void)
{
    utility_panel = ViewHostSystem_Create(&utility_panel_viewport, 1.0f, (Vector2d){5.0f, 5.0f},
        UIPalette_GetActive(), ui_standard_stack_spacing);
    if (!utility_panel)
    {
        return;
    }

    // Utility now contains telemetry only; debug controls live in the toolbar and debug overlay hotkeys.
    ViewHostSystem_InitViews(utility_panel, 1);
    ViewHostSystem_InitRoot(utility_panel);
    InitUtilityStatsView();

    // Finalise: update layout (no view selector for utility panel)
    ViewHostSystem_FinaliseInit(utility_panel, NULL);
}

// Destroy the utility panel and clear its cached UI references.
void DestroyUtilityPanel(void)
{
    ViewHostSystem *panel = utility_panel;
    utility_panel = NULL;
    ViewHostSystem_Destroy(panel);

    G_UIState.stats_polygs_str = NULL;
    G_UIState.stats_fps_str = NULL;
    G_UIState.stats_ftime_str = NULL;
    G_UIState.stats_mem_str = NULL;
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
