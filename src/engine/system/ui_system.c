/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "ui/ui_renderer.h"
#include "ui/ui_input.h"
#include "system/ui_system.h"
#include "system/ui/world_manager_system.h"
#include "system/ui/state_manager_system.h"
#include "system/view_host_system.h"
#include "system/ui/utility_panel_system.h"
#include "entities/entity_factory.h"
#include "system/ui/popup_menu.h"
#include "system/ui/info_strip_system.h"
#include "system/ui/toolbar_system.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "input/drag_interaction.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "system/debug_overlay_system.h"
#include "memory/cmemory.h"
#include "system/ui/ui_loader.h"
#include "world/target_resolver.h"
#include "entities/entity_registry.h"
#include "entities/entity_catalogue.h"
#include "system/ui/detail_strip_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
UIState G_UIState = {0};
bool ui_borders_enabled = false;

// Currently active tool for the vertical tool toolbar. Radio-exclusive: at most one tool is
// active at a time, defaulting to TOOL_NONE (no additive tool action runs).
static ActiveTool g_active_tool = TOOL_NONE;

// Active tile type for the cell-paint tool. Module-owned like g_active_tool: a single small
// integer the paint-commit reads to stamp painted cells and the cycle toolbar button advances.
// Defaults to 1 (the first non-default, visibly-coloured palette type) so a fresh paint click
// produces an immediately visible colour change rather than repainting with the grid's own fill
// (tile_type 0 renders as the grid colour, i.e. identical to an unpainted cell). The value is
// always kept within the palette range by UIState_SetActiveTileType.
static int g_active_tile_type = 1;

// Active prefab for the spawn tool. Module-owned exactly like g_active_tile_type: a single small
// index the spawn commit reads (to pick the catalogue entry) and the cycle toolbar button advances.
// Defaults to 0 (the first catalogue entry). Always kept within the catalogue range by
// UIState_SetActivePrefab; when the catalogue is empty it stays 0 and spawning is a no-op.
static int g_active_prefab = 0;

// Independent on/off state for the in-world geometry (vertex) editor. NOT part of the
// radio-exclusive tool set: toggling it never changes g_active_tool, and it is not a debug
// overlay. GeometryEditor_IsEnabled reads it (together with a non-NULL selection) as the single
// gate for vertex-handle input and rendering. Defaults OFF.
static bool g_geometry_edit_active = false;
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
// All UI layout values below are in UI pixels (drawn at the integer UI pixel scale).
const Vector2d ui_standard_container_padding = {3.0f, 3.0f};
// Field rows and titles start exactly at the content edge of their section.
const Vector2d ui_standard_field_padding = {0.0f, 0.0f};
// Section padding: 6 px, measured from the box's outer edge (the 1 px border
// is drawn inside the box, so content sits 6 px from the outer edge as in the mock).
const Vector2d ui_section_padding = {6.0f, 6.0f};
// ViewHost / View padding on top of the 5 px root padding: 5 + 1 = 6 px panel inset.
const Vector2d ui_host_padding = {1.0f, 1.0f};
// Root padding of the single-row read-only strips (info, detail): the 5 px panel root inset on the
// x axis only, so the fields clear the band's left and right edges without eating into the band's
// 25 px row height.
const Vector2d ui_strip_host_padding = {5.0f, 0.0f};
const Vector2d ui_standard_button_padding = {1.0f, 1.0f};
// Horizontal padding each side of a tab or tab-like option. Tabs hug their text, so width = text +
// 2 * this (rule R-1). Wider than the 1 px button padding, which would leave tab text touching its edges.
const Vector2d ui_standard_tab_padding = {3.0f, 1.0f};
// Text-field rows (and Textbox/Label defaults) fill their parent's width; height stays 25 px.
const Size ui_standard_control_size = {{233.0f, 25.0f}, AXIS_FILL, AXIS_FIXED};
// The input FILLS the row after the label column (the label is a fixed-width column).
const Size ui_standard_textfield_input_size = {{1.0f, 1.0f}, AXIS_FILL, AXIS_PERCENT};
// First-frame / fallback label column width: 118 px ("Restitution" run + 2 px).
const float ui_standard_textfield_label_width = 118.0f;
// Buttons span the full inner width of their container; height is 25 px.
const Size ui_standard_button_size = {{161.0f, 25.0f}, AXIS_FILL, AXIS_FIXED};
// Icon-only tool and toggle buttons (toolbar): 34 px wide, one standard 25 px control high.
const Size ui_standard_icon_button_size = {{34.0f, 25.0f}, AXIS_FIXED, AXIS_FIXED};
// Shared size for container title labels: width HUGS the title's measured text (uncapped,
// dimensions.x == 0) so the title can never be clipped; the height is one 16 px FONT_LARGE row
// so, with 6 px padding and a 3 px gap, the first control lands 25 px below the box top.
const Size ui_title_size = {{0.0f, 16.0f}, AXIS_HUG, AXIS_FIXED};
const Size ui_standard_container_size = {{1, 0.5}, AXIS_PERCENT, AXIS_PERCENT};
const Size ui_fill_container_size = {{1.0f, 1.0f}, AXIS_FILL, AXIS_FILL};
const Size ui_standard_selector_container_size = UI_SIZE_HUG;
const Size ui_standard_selector_button_size = {{88.0f, 25.0f}, AXIS_FIXED, AXIS_FIXED};
// NOTE: the middle field of each Layout literal is NumberForm (NONE), NOT an AxisSizeMode.
const Layout ui_standard_stack_spacing = {{0.0f, 3.0f}, NONE, FLOW_STACKED};
// 6 px between a panel's top-level blocks (header, tab row, sections).
const Layout ui_view_stack_spacing = {{0.0f, 6.0f}, NONE, FLOW_STACKED};
const Layout ui_standard_stack_wrap_spacing = {{3.0f, 3.0f}, NONE, FLOW_STACKED_WRAP};
const Layout ui_standard_inline_wrap_spacing = {{3.0f, 3.0f}, NONE, FLOW_INLINE_WRAP};
const Layout ui_standard_inline_spacing = {{3.0f, 0.0f}, NONE, FLOW_INLINE};
const Layout ui_zero_inline_spacing = {{0.0f, 0.0f}, NONE, FLOW_INLINE};

// Border thickness every themed surface role starts with (1 screen pixel, the historic look).
#define UI_DEFAULT_BORDER_WEIGHT 1

UIPalette ui_classic_palette = {0};
UIPalette ui_earth_palette = {0};
UIPalette ui_harbor_palette = {0};
UIPalette ui_meadow_palette = {0};
UIPalette ui_default_palette = {0};

static bool ui_palettes_initialized = false;
static UIStateSelectionChangedCallback ui_selection_changed_callback = NULL;
static void *ui_selection_changed_user_data = NULL;

static ColourRgba MakeColour(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    ColourRgba c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static void InitUIPalettes(void)
{
    if (ui_palettes_initialized)
    {
        return;
    }

    ui_classic_palette.panel_background = MakeColour(32, 42, 37, 255);
    ui_classic_palette.container_border = MakeColour(194, 105, 83, 255);
    ui_classic_palette.container_fill = MakeColour(111, 137, 119, 255);
    ui_classic_palette.field_row_border = MakeColour(255, 255, 255, 255);
    ui_classic_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_classic_palette.input_border = MakeColour(32, 42, 37, 255);
    ui_classic_palette.input_fill = MakeColour(245, 241, 230, 255);
    ui_classic_palette.button_border = MakeColour(32, 42, 37, 255);
    ui_classic_palette.container_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_classic_palette.field_row_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_classic_palette.input_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_classic_palette.button_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_classic_palette.button_fill = MakeColour(230, 194, 119, 255);
    // Muted text (x/y prefixes, unit suffix): brown on the cream input, independent of the control roles.
    ui_classic_palette.muted_text = MakeColour(122, 100, 55, 255);
    // Danger fill: the palette's terracotta container border.
    ui_classic_palette.button_danger_fill = MakeColour(194, 105, 83, 255);
    ui_classic_palette.text = MakeColour(10, 16, 12, 255);
    // Field labels: near-black tuned for container_fill (WCAG AA); card ink uses text_on_dark.
    ui_classic_palette.label_text = MakeColour(6, 12, 8, 255);
    ui_classic_palette.title_text = MakeColour(0, 0, 0, 255);
    // Headers keep dark ink on a lighter container-hued fill; read-only fields use a dimmed input fill.
    ui_classic_palette.header_text = MakeColour(0, 0, 0, 255);
    ui_classic_palette.header_fill = MakeColour(214, 236, 224, 255);
    ui_classic_palette.readonly_fill = MakeColour(213, 207, 188, 255);
    ui_classic_palette.readonly_border = MakeColour(110, 128, 118, 255);
    ui_classic_palette.subsection_fill = MakeColour(78, 100, 86, 255);
    ui_classic_palette.card_fill = MakeColour(78, 100, 86, 255);
    ui_classic_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_classic_palette.error = MakeColour(230, 41, 55, 255);
    ui_classic_palette.warning = MakeColour(255, 255, 176, 255);
    // Translucent dark backing for the diagnostics dashboard.
    ui_classic_palette.overlay_fill = MakeColour(8, 12, 18, 215);
    // Outline drawn around every element while the UI-borders debug overlay is on.
    ui_classic_palette.debug_outline = MakeColour(255, 255, 255, 80);

    ui_earth_palette.panel_background = MakeColour(38, 101, 115, 255);
    ui_earth_palette.container_border = MakeColour(191, 78, 36, 255);
    ui_earth_palette.container_fill = MakeColour(121, 120, 49, 255);
    ui_earth_palette.field_row_border = MakeColour(166, 109, 60, 255);
    ui_earth_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_earth_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.input_fill = MakeColour(242, 211, 172, 255);
    ui_earth_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.container_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_earth_palette.field_row_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_earth_palette.input_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_earth_palette.button_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_earth_palette.button_fill = MakeColour(166, 109, 60, 255);
    // Muted text (x/y prefixes, unit suffix): brown on the cream input, independent of the control roles.
    ui_earth_palette.muted_text = MakeColour(112, 74, 41, 255);
    // Danger fill: the palette's rust container border.
    ui_earth_palette.button_danger_fill = MakeColour(214, 96, 52, 255);
    ui_earth_palette.text = MakeColour(0, 0, 0, 255);
    // Black is the only ink that passes on the olive container.
    ui_earth_palette.label_text = MakeColour(0, 0, 0, 255);
    ui_earth_palette.title_text = MakeColour(0, 0, 0, 255);
    // Headers keep dark ink on a lighter container-hued fill; read-only fields use a dimmed input fill.
    ui_earth_palette.header_text = MakeColour(0, 0, 0, 255);
    ui_earth_palette.header_fill = MakeColour(196, 190, 120, 255);
    ui_earth_palette.readonly_fill = MakeColour(205, 183, 150, 255);
    ui_earth_palette.readonly_border = MakeColour(110, 112, 60, 255);
    ui_earth_palette.subsection_fill = MakeColour(90, 89, 36, 255);
    ui_earth_palette.card_fill = MakeColour(90, 89, 36, 255);
    ui_earth_palette.text_on_dark = MakeColour(242, 211, 172, 255);
    ui_earth_palette.error = MakeColour(230, 41, 55, 255);
    ui_earth_palette.warning = MakeColour(255, 255, 176, 255);
    // Translucent dark backing for the diagnostics dashboard.
    ui_earth_palette.overlay_fill = MakeColour(8, 12, 18, 215);
    // Outline drawn around every element while the UI-borders debug overlay is on.
    ui_earth_palette.debug_outline = MakeColour(255, 255, 255, 80);

    ui_harbor_palette.panel_background = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.container_border = MakeColour(218, 117, 96, 255);
    // Lighter sage section fill (#78AA9E); the dark body ink below reaches 7.1:1 on it.
    ui_harbor_palette.container_fill = MakeColour(120, 170, 158, 255);
    ui_harbor_palette.field_row_border = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_harbor_palette.input_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.input_fill = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.button_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.container_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_harbor_palette.field_row_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_harbor_palette.input_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_harbor_palette.button_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_harbor_palette.button_fill = MakeColour(220, 158, 70, 255);
    // Muted text (x/y prefixes, unit suffix): brown on the cream input, independent of the control roles.
    ui_harbor_palette.muted_text = MakeColour(124, 87, 33, 255);
    // Danger fill: the mock's coral (equal to the container border).
    ui_harbor_palette.button_danger_fill = MakeColour(218, 117, 96, 255);
    // One dark ink (#0A1418, near-black dark teal) for body text and field labels: 7.1:1 on the
    // #78AA9E container fill and 13.9:1 on the cream input fill.
    ui_harbor_palette.text = MakeColour(10, 20, 24, 255);
    ui_harbor_palette.label_text = MakeColour(10, 20, 24, 255);
    // Section titles: the same dark ink on the sage sheet (next to an amber accent bar).
    ui_harbor_palette.title_text = MakeColour(10, 20, 24, 255);
    // Panel headers: black ink on a lighter sage (#9CC4B9) fill (amber in multi-select).
    ui_harbor_palette.header_text = MakeColour(0, 0, 0, 255);
    ui_harbor_palette.header_fill = MakeColour(156, 196, 185, 255);
    // Read-only fields: dimmed cream (#CFC5A8) with a muted sage border (#6E8F86); ink stays #0A1418.
    ui_harbor_palette.readonly_fill = MakeColour(207, 197, 168, 255);
    ui_harbor_palette.readonly_border = MakeColour(110, 143, 134, 255);
    // Darker than container_fill; light ink (text_on_dark) reaches 4.84:1 on it.
    ui_harbor_palette.subsection_fill = MakeColour(58, 102, 94, 255);
    // CARD role: same dark teal as INACTIVE today (#3A665E), a separate field so they can diverge.
    ui_harbor_palette.card_fill = MakeColour(58, 102, 94, 255);
    ui_harbor_palette.text_on_dark = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.error = MakeColour(230, 41, 55, 255);
    ui_harbor_palette.warning = MakeColour(255, 255, 176, 255);
    // Translucent dark backing for the diagnostics dashboard.
    ui_harbor_palette.overlay_fill = MakeColour(8, 12, 18, 215);
    // Outline drawn around every element while the UI-borders debug overlay is on.
    ui_harbor_palette.debug_outline = MakeColour(255, 255, 255, 80);

    ui_meadow_palette.panel_background = MakeColour(121, 120, 49, 255);
    ui_meadow_palette.container_border = MakeColour(230, 194, 119, 255);
    ui_meadow_palette.container_fill = MakeColour(111, 137, 119, 255);
    ui_meadow_palette.field_row_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_meadow_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.input_fill = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.container_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_meadow_palette.field_row_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_meadow_palette.input_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_meadow_palette.button_border_weight = UI_DEFAULT_BORDER_WEIGHT;
    ui_meadow_palette.button_fill = MakeColour(230, 194, 119, 255);
    // Muted text (x/y prefixes, unit suffix): brown on the cream input, independent of the control roles.
    ui_meadow_palette.muted_text = MakeColour(122, 100, 55, 255);
    // Danger fill: a dedicated terracotta, because meadow's container border is the button amber.
    ui_meadow_palette.button_danger_fill = MakeColour(196, 92, 70, 255);
    ui_meadow_palette.text = MakeColour(10, 16, 12, 255);
    ui_meadow_palette.label_text = MakeColour(0, 18, 26, 255);
    ui_meadow_palette.title_text = MakeColour(0, 0, 0, 255);
    // Headers keep dark ink on a lighter container-hued fill; read-only fields use a dimmed input fill.
    ui_meadow_palette.header_text = MakeColour(0, 0, 0, 255);
    ui_meadow_palette.header_fill = MakeColour(190, 214, 196, 255);
    ui_meadow_palette.readonly_fill = MakeColour(213, 207, 188, 255);
    ui_meadow_palette.readonly_border = MakeColour(96, 140, 130, 255);
    ui_meadow_palette.subsection_fill = MakeColour(78, 100, 86, 255);
    ui_meadow_palette.card_fill = MakeColour(78, 100, 86, 255);
    ui_meadow_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.error = MakeColour(230, 41, 55, 255);
    ui_meadow_palette.warning = MakeColour(255, 255, 176, 255);
    // Translucent dark backing for the diagnostics dashboard.
    ui_meadow_palette.overlay_fill = MakeColour(8, 12, 18, 215);
    // Outline drawn around every element while the UI-borders debug overlay is on.
    ui_meadow_palette.debug_outline = MakeColour(255, 255, 255, 80);

    ui_default_palette = ui_harbor_palette;
    ui_palettes_initialized = true;
}

// Resolve a universal colour role to its fill / text / border triple. This is the single place
// that pairs every fill with the text colour drawn on it, so views and controls never pick their
// own colours. A NULL palette falls back to the default.
UIRoleColours UIPalette_GetRoleColours(const UIPalette *palette, UIColourRole role)
{
    if (!palette)
    {
        palette = &ui_default_palette;
    }

    UIRoleColours colours = { COLOURLESS_RGBA, palette->text, COLOURLESS_RGBA };
    switch (role)
    {
        case UI_ROLE_SHEET:
            colours.fill = palette->container_fill;
            colours.border = palette->container_border;
            break;
        case UI_ROLE_HEADER:
            colours.fill = palette->header_fill;
            colours.text = palette->header_text;
            break;
        case UI_ROLE_INPUT:
            colours.fill = palette->input_fill;
            colours.border = palette->input_border;
            break;
        case UI_ROLE_READONLY_INPUT:
            colours.fill = palette->readonly_fill;
            colours.border = palette->readonly_border;
            break;
        case UI_ROLE_INACTIVE:
            // Dark teal with light text: tabs and OFF toggles / chips.
            colours.fill = palette->subsection_fill;
            colours.text = palette->text_on_dark;
            colours.border = palette->input_border;
            break;
        case UI_ROLE_CARD:
            // Grouped-field card: dark teal with cream text. Its own palette field, so it can
            // diverge from INACTIVE later.
            colours.fill = palette->card_fill;
            colours.text = palette->text_on_dark;
            colours.border = palette->input_border;
            break;
        case UI_ROLE_ACTIVE:
            colours.fill = palette->button_fill;
            colours.border = palette->button_border;
            break;
        case UI_ROLE_DANGER:
            colours.fill = palette->button_danger_fill;
            colours.border = palette->button_border;
            break;
    }
    return colours;
}

// Which palette border weight a surface uses.
typedef enum SurfaceWeightSource
{
    SURFACE_WEIGHT_NONE,       // Borderless surface.
    SURFACE_WEIGHT_CONTAINER,
    SURFACE_WEIGHT_FIELD_ROW,
    SURFACE_WEIGHT_INPUT,
    SURFACE_WEIGHT_BUTTON
} SurfaceWeightSource;

// One row per UIPaletteSurface: the colour role that supplies fill and border (or none, for the
// surfaces that read palette fields directly), which palette weight gives the border thickness, and
// whether the surface brings the role's text colour with it (control roles only). Indexed by the
// surface enum, so a new surface cannot be added without a row here.
typedef struct SurfaceMapRow
{
    bool has_role;
    UIColourRole role;
    SurfaceWeightSource weight;
    bool has_text;
} SurfaceMapRow;

static const SurfaceMapRow s_surface_map[] = {
    [UI_PALETTE_SURFACE_TRANSPARENT] = {false, UI_ROLE_SHEET,    SURFACE_WEIGHT_NONE,      false},
    [UI_PALETTE_SURFACE_CONTAINER]   = {true,  UI_ROLE_SHEET,    SURFACE_WEIGHT_CONTAINER, false},
    [UI_PALETTE_SURFACE_FIELD_ROW]   = {false, UI_ROLE_SHEET,    SURFACE_WEIGHT_FIELD_ROW, false},
    [UI_PALETTE_SURFACE_INPUT]       = {true,  UI_ROLE_INPUT,    SURFACE_WEIGHT_INPUT,     false},
    [UI_PALETTE_SURFACE_BUTTON]      = {true,  UI_ROLE_ACTIVE,   SURFACE_WEIGHT_BUTTON,    true},
    [UI_PALETTE_SURFACE_PANEL]       = {false, UI_ROLE_SHEET,    SURFACE_WEIGHT_NONE,      false},
    [UI_PALETTE_SURFACE_DANGER]      = {true,  UI_ROLE_DANGER,   SURFACE_WEIGHT_BUTTON,    true},  // danger shares the button border
    [UI_PALETTE_SURFACE_INACTIVE]    = {true,  UI_ROLE_INACTIVE, SURFACE_WEIGHT_INPUT,     true},  // inactive tiles share the input border
    [UI_PALETTE_SURFACE_CARD]        = {true,  UI_ROLE_CARD,     SURFACE_WEIGHT_INPUT,     true},  // cards share the input border
};

// Resolve a surface= name to its fill, border, border weight and paired text colour from the
// s_surface_map table. Role-backed surfaces take fill and border (and text, where the row says so)
// from UIPalette_GetRoleColours so the pairings live in one place. The two surfaces with no role
// read palette fields directly: field_row (its own border and fill) and panel (dark background, no
// border). Unknown surfaces resolve to the transparent style. A NULL palette uses the default.
UISurfaceStyle UIPalette_GetSurfaceStyle(const UIPalette *palette, UIPaletteSurface surface)
{
    UISurfaceStyle style = {COLOURLESS_RGBA, COLOURLESS_RGBA, 0, false, COLOURLESS_RGBA};

    if (!palette)
    {
        palette = &ui_default_palette;
    }
    if ((size_t)surface >= ARRAY_COUNT(s_surface_map))
    {
        return style;
    }

    const SurfaceMapRow *row = &s_surface_map[surface];
    if (row->has_role)
    {
        UIRoleColours colours = UIPalette_GetRoleColours(palette, row->role);
        style.fill = colours.fill;
        style.border = colours.border;
        if (row->has_text)
        {
            style.has_text = true;
            style.text = colours.text;
        }
    }
    else if (surface == UI_PALETTE_SURFACE_FIELD_ROW)
    {
        style.border = palette->field_row_border;
        style.fill = palette->field_row_fill;
    }
    else if (surface == UI_PALETTE_SURFACE_PANEL)
    {
        style.fill = palette->panel_background;
    }

    switch (row->weight)
    {
        case SURFACE_WEIGHT_CONTAINER: style.border_weight = palette->container_border_weight; break;
        case SURFACE_WEIGHT_FIELD_ROW: style.border_weight = palette->field_row_border_weight; break;
        case SURFACE_WEIGHT_INPUT:     style.border_weight = palette->input_border_weight; break;
        case SURFACE_WEIGHT_BUTTON:    style.border_weight = palette->button_border_weight; break;
        case SURFACE_WEIGHT_NONE:
        default:                       break;
    }
    return style;
}

// Apply a colour role to an element: fill and border always, and the role's text colour on the
// element's own font (buttons and labels). The single entry point for panels that must recolour
// a live element (popup rows, chips, tabs), so none of them pair fill and text by hand.
// The one context rule: an INACTIVE control flagged in_card would be dark teal on dark teal, so it
// resolves to the SHEET fill with ink text and keeps the INACTIVE border. ACTIVE, DANGER and every
// other role are unchanged by context. A NULL element or palette is a no-op.
void UIElement_ApplyRoleColours(UIElement *element, const UIPalette *palette, UIColourRole role)
{
    if (!element || !palette)
    {
        return;
    }
    UIRoleColours colours = UIPalette_GetRoleColours(palette, role);
    if (element->in_card && role == UI_ROLE_INACTIVE)
    {
        UIRoleColours sheet = UIPalette_GetRoleColours(palette, UI_ROLE_SHEET);
        colours.fill = sheet.fill;
        colours.text = sheet.text;
    }
    element->colour_fill = colours.fill;
    element->colour_border = colours.border;
    if (IsBtn(element))
    {
        element->data.button.font.colour = colours.text;
    }
    else if (element->type == UI_ELEMENT_LABEL)
    {
        element->data.label.font.colour = colours.text;
    }
}

// Flag every descendant of `root` as inside a card (recursive over the child links). Run once when
// a card subtree is built, so UIElement_ApplyRoleColours reads a flag instead of walking parents.
void UIElement_MarkDescendantsInCard(UIElement *root)
{
    if (!root)
    {
        return;
    }
    ForEachChild(root, child)
    {
        child->in_card = true;
        UIElement_MarkDescendantsInCard(child);
    }
}
// Returns the live palette storage. InitUIPalettes is idempotent, so a call made before InitUI
// still yields the harbour default rather than a zeroed palette.
const UIPalette *UIPalette_GetActive(void)
{
    InitUIPalettes();
    return &ui_default_palette;
}

// Copies `src` into the live palette storage (the address never changes, so every palette pointer
// already handed out stays valid) and rebuilds the UI, because element colours are copied by value
// at construction. A UI that does not exist yet only stores the palette.
void UIPalette_SetActive(const UIPalette *src)
{
    if (!src || src == &ui_default_palette)
    {
        return;
    }

    // Ensure the one-off init has run first so it cannot overwrite the palette copied below.
    InitUIPalettes();
    ui_default_palette = *src;

    // The element pool exists only between the first UI allocation and DestroyUI.
    if (GetUIElementPool() != NULL)
    {
        ResetUI();
    }
}

// UI Elements

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void UpdateGlobalUIState();

//----------------------------------------------------------------------------------
// Central SelectionSet (mixed entity/world/cell TargetRef membership)
//----------------------------------------------------------------------------------

// Lazily create the backing LArray on first use. G_UIState is a {0} global with no explicit
// teardown and InitUI may re-run on screen transitions, so a {0} array (elem_bytes == 0) must
// never be pushed to; this guards that without per-frame allocation.
static void SelectionSet_EnsureInit(void)
{
    if (G_UIState.selection_set.elem_bytes == 0)
    {
        // A small initial capacity covers the common 1-3 member case without churn.
        G_UIState.selection_set = MakeLArray(4, sizeof(TargetRef));
    }
}

// Identity for "same member": entities match by id, worlds by index, cells by owning world and
// cell index. A TARGET_KIND_NONE ref never equals anything (including another NONE).
static bool SelectionSet_RefsEqual(TargetRef a, TargetRef b)
{
    if (a.kind != b.kind || a.kind == TARGET_KIND_NONE)
    {
        return false;
    }

    switch (a.kind)
    {
        case TARGET_KIND_ENTITY:
            return a.as.entity == b.as.entity;
        case TARGET_KIND_WORLD:
            return a.as.world_index == b.as.world_index;
        case TARGET_KIND_CELL:
            return a.as.cell.world_index == b.as.cell.world_index &&
                   a.as.cell.lattice_x == b.as.cell.lattice_x &&
                   a.as.cell.lattice_y == b.as.cell.lattice_y;
        default:
            return false;
    }
}

// Empty the selection set, retaining its allocated capacity for reuse.
void SelectionSet_Clear(void)
{
    SelectionSet_EnsureInit();
    ClearLArray(&G_UIState.selection_set);
}

// Report whether an equal ref is already a member of the set.
bool SelectionSet_Contains(TargetRef ref)
{
    SelectionSet_EnsureInit();
    TargetRef *member = NULL;
    LArray_ForEach(&G_UIState.selection_set, TargetRef*, member)
    {
        if (SelectionSet_RefsEqual(*member, ref))
        {
            return true;
        }
    }
    return false;
}

// Return the number of members currently in the set.
int SelectionSet_Count(void)
{
    SelectionSet_EnsureInit();
    return LArray_Count(&G_UIState.selection_set);
}

// Add a member to the set. TARGET_KIND_NONE is ignored so a failed pick never adds junk; an
// already-present ref is a no-op so the collection stays a true set.
void SelectionSet_Add(TargetRef ref)
{
    SelectionSet_EnsureInit();
    if (ref.kind == TARGET_KIND_NONE || SelectionSet_Contains(ref))
    {
        return;
    }
    // On allocation failure LArray_Push leaves the set unchanged; the add simply does not take
    // effect this frame, which is recoverable (no partial state to unwind).
    LArray_Push(&G_UIState.selection_set, &ref);
}

// Remove an equal member if present. Uses order-preserving removal (not swap-pop) so removing a
// non-primary member does not silently promote an arbitrary element to primary. Returns whether
// a member was removed.
bool SelectionSet_Remove(TargetRef ref)
{
    SelectionSet_EnsureInit();
    for (int index = 0; index < G_UIState.selection_set.count; index++)
    {
        TargetRef *member = (TargetRef *)LArray_Get(&G_UIState.selection_set, index);
        if (member && SelectionSet_RefsEqual(*member, ref))
        {
            return LArray_RemoveAt(&G_UIState.selection_set, index);
        }
    }
    return false;
}

// Toggle a member: remove it if present, otherwise add it.
void SelectionSet_Toggle(TargetRef ref)
{
    if (SelectionSet_Contains(ref))
    {
        SelectionSet_Remove(ref);
    }
    else
    {
        SelectionSet_Add(ref);
    }
}

// Copy the member at `index` into *out. Returns false when out of range. Does not allocate, so it
// is safe to call per member in a loop.
bool SelectionSet_Get(int index, TargetRef *out)
{
    SelectionSet_EnsureInit();
    if (!out || index < 0 || index >= G_UIState.selection_set.count)
    {
        return false;
    }
    TargetRef *member = (TargetRef *)LArray_Get(&G_UIState.selection_set, index);
    if (!member)
    {
        return false;
    }
    *out = *member;
    return true;
}

// Return the primary (last-added) member, or a TARGET_KIND_NONE ref when the set is empty.
TargetRef SelectionSet_Primary(void)
{
    SelectionSet_EnsureInit();
    int count = G_UIState.selection_set.count;
    if (count <= 0)
    {
        return (TargetRef){ .kind = TARGET_KIND_NONE };
    }
    TargetRef *primary = (TargetRef *)LArray_Get(&G_UIState.selection_set, count - 1);
    return primary ? *primary : (TargetRef){ .kind = TARGET_KIND_NONE };
}

void UIState_SetSelectedObject(Newtonoid2d *object)
{
    // Centralise object selection writes so other systems stop mutating the UI state directly.
    UIState_SetSelection(object, G_UIState.selected_cell, G_UIState.selected_cell_index);
}

void UIState_SetSelectedObjectById(EntityId entity_id)
{
    // Resolve by stable ID at write time to avoid leaking cross-module pointer lookup details.
    if (entity_id == INVALID_ENTITY_ID)
    {
        UIState_SetSelectedObject(NULL);
        return;
    }

    Newtonoid2d *selected_object = Universe_GetEntityByID(&G_Universe, entity_id, NULL);
    UIState_SetSelectedObject(selected_object);
}

void UIState_SetSelection(Newtonoid2d *object, Cell *cell, int cell_index)
{
    // Keep object and cell selection updates in one write path to avoid partial state updates.
    EntityId prev_selected_object_id = G_UIState.selected_object_id;
    Newtonoid2d *prev_selected_object = G_UIState.selected_object;
    Cell *prev_selected_cell = G_UIState.selected_cell;
    int prev_selected_cell_index = G_UIState.selected_cell_index;

    G_UIState.selected_object_id = object ? object->id : INVALID_ENTITY_ID;
    G_UIState.selected_object = object;
    G_UIState.selected_cell = cell;
    G_UIState.selected_cell_index = cell_index;

    // Capture the context world BEFORE the clear below: it is derived from the set's primary, so an
    // emptied set would always report ROOT.
    int ref_world_index = UIState_GetContextWorldIndex();

    // Mirror a single-selection replace into the central set: clear it, then add the one member
    // this write represents (entity, else cell, else nothing). This makes every existing
    // single-select caller (which funnels through here) transparently drive the set.
    SelectionSet_Clear();
    if (object)
    {
        SelectionSet_Add((TargetRef){ .kind = TARGET_KIND_ENTITY, .as.entity = object->id });
    }
    else if (cell_index >= 0)
    {
        // A cell-only selection records a CELL ref for the context world captured above,
        // matching the info strip's existing owning-world resolution.
        World2d *ref_world = Universe_GetWorld(&G_Universe, ref_world_index);
        // Convert the caller's dense index to an ABSOLUTE lattice coordinate (grid_origin + column/row),
        // because a dense index goes stale when the world grows but a lattice coordinate does not.
        if (ref_world && ref_world->grid_space.space.columns > 0 &&
            cell_index < ref_world->grid_space.space.cells.count)
        {
            const Space2d *ref_space = &ref_world->grid_space.space;
            SelectionSet_Add((TargetRef){
                .kind = TARGET_KIND_CELL,
                .as.cell = {
                    .world_index = ref_world_index,
                    .lattice_x = (int)ref_space->grid_origin.x + (cell_index % ref_space->columns),
                    .lattice_y = (int)ref_space->grid_origin.y + (cell_index / ref_space->columns),
                },
            });
        }
    }
    // A full clear (object == NULL, cell_index < 0) leaves the set empty.

    bool selection_changed = prev_selected_object_id != G_UIState.selected_object_id ||
                            prev_selected_object != G_UIState.selected_object ||
                            prev_selected_cell != G_UIState.selected_cell ||
                            prev_selected_cell_index != G_UIState.selected_cell_index;
    if (selection_changed && ui_selection_changed_callback)
    {
        ui_selection_changed_callback(G_UIState.selected_object_id,
                                      G_UIState.selected_object,
                                      G_UIState.selected_cell,
                                      G_UIState.selected_cell_index,
                                      ui_selection_changed_user_data);
    }
}

// Derive the context world index from the SelectionSet primary (no stored copy): a world primary is
// that world, an entity primary is its owning world, a cell primary is the cell's world. An empty
// set, or a primary that no longer resolves to a live world, yields the root sentinel.
int UIState_GetContextWorldIndex(void)
{
    TargetRef primary = SelectionSet_Primary();
    int world_index = UNIVERSE_ROOT_WORLD_INDEX;

    switch (primary.kind)
    {
        case TARGET_KIND_WORLD:
            world_index = primary.as.world_index;
            break;
        case TARGET_KIND_ENTITY:
            // Universe_GetEntityByID reports the owning world index (-1 when unresolved).
            if (!Universe_GetEntityByID(&G_Universe, primary.as.entity, &world_index))
            {
                world_index = UNIVERSE_ROOT_WORLD_INDEX;
            }
            break;
        case TARGET_KIND_CELL:
            world_index = primary.as.cell.world_index;
            break;
        case TARGET_KIND_NONE:
        default:
            break;
    }

    // Stale or unresolved indices (including -1) fall back to root.
    return Universe_GetWorld(&G_Universe, world_index) ? world_index : UNIVERSE_ROOT_WORLD_INDEX;
}

// Resolve the context world; never NULL because the index always falls back to the root world.
World2d *UIState_GetContextWorld(void)
{
    return Universe_GetWorld(&G_Universe, UIState_GetContextWorldIndex());
}

// Re-derive the single-valued primary fields (selected_object_id / selected_object) from the
// set's primary member and fire the change callback when they differ. Called after set toggles
// and per-frame pruning so the back-compat getters and callback track the primary. A non-entity
// or empty primary clears the entity single-fields; cell single-fields are left untouched here
// (cell selection is still driven through UIState_SetSelection).
void UIState_SyncPrimaryFieldsFromSet(void)
{
    EntityId prev_selected_object_id = G_UIState.selected_object_id;
    Newtonoid2d *prev_selected_object = G_UIState.selected_object;

    TargetRef primary = SelectionSet_Primary();
    if (primary.kind == TARGET_KIND_ENTITY)
    {
        G_UIState.selected_object_id = primary.as.entity;
        G_UIState.selected_object = Universe_GetEntityByID(&G_Universe, primary.as.entity, NULL);
    }
    else
    {
        G_UIState.selected_object_id = INVALID_ENTITY_ID;
        G_UIState.selected_object = NULL;
    }

    bool selection_changed = prev_selected_object_id != G_UIState.selected_object_id ||
                             prev_selected_object != G_UIState.selected_object;
    if (selection_changed && ui_selection_changed_callback)
    {
        ui_selection_changed_callback(G_UIState.selected_object_id,
                                      G_UIState.selected_object,
                                      G_UIState.selected_cell,
                                      G_UIState.selected_cell_index,
                                      ui_selection_changed_user_data);
    }
}

void UIState_SetSelectionChangedCallback(UIStateSelectionChangedCallback callback, void *user_data)
{
    ui_selection_changed_callback = callback;
    ui_selection_changed_user_data = user_data;
}

void UIState_ClearSelectedObject(void)
{
    UIState_SetSelectedObject(NULL);
}

void UIState_ValidateSelection(void)
{
    // Keep pointer state synchronised with the selected ID to prevent stale references after deletes/moves.
    if (G_UIState.selected_object_id == INVALID_ENTITY_ID)
    {
        if (G_UIState.selected_object)
        {
            UIState_SetSelectedObject(NULL);
        }
        return;
    }

    Newtonoid2d *selected_object = Universe_GetEntityByID(&G_Universe, G_UIState.selected_object_id, NULL);
    if (!selected_object)
    {
        UIState_SetSelectedObject(NULL);
        return;
    }

    if (selected_object != G_UIState.selected_object)
    {
        UIState_SetSelectedObject(selected_object);
    }
}

// Prune stale members from the selection set so dead ids are purged after deletes/moves that go
// through the existing single paths. Runs each frame alongside the single-pointer re-sync above;
// it only reacts to deletions, it never deletes anything itself. Iterates by index because it
// removes while iterating.
void UIState_ValidateSelectionSet(void)
{
    SelectionSet_EnsureInit();

    bool pruned = false;
    int index = 0;
    while (index < G_UIState.selection_set.count)
    {
        TargetRef *member = (TargetRef *)LArray_Get(&G_UIState.selection_set, index);
        bool stale = false;
        if (member)
        {
            switch (member->kind)
            {
                case TARGET_KIND_ENTITY:
                    // Drop entities whose id is no longer live.
                    stale = !EntityRegistry_IsIdActive(member->as.entity) ||
                            Universe_GetEntityByID(&G_Universe, member->as.entity, NULL) == NULL;
                    break;
                case TARGET_KIND_WORLD:
                    // Drop worlds whose index is out of range (the root sentinel stays valid).
                    stale = member->as.world_index != UNIVERSE_ROOT_WORLD_INDEX &&
                            (member->as.world_index < 0 ||
                             member->as.world_index >= Universe_GetWorldCount(&G_Universe));
                    break;
                case TARGET_KIND_CELL:
                    // Validate a cell via its owning world index, then confirm the lattice coordinate
                    // still lies inside that world's grid (a trim shrinks it, dropping selections in the removed
                    // absent margin).
                    stale = member->as.cell.world_index != UNIVERSE_ROOT_WORLD_INDEX &&
                            (member->as.cell.world_index < 0 ||
                             member->as.cell.world_index >= Universe_GetWorldCount(&G_Universe));
                    if (!stale)
                    {
                        World2d *member_world = Universe_GetWorld(&G_Universe, member->as.cell.world_index);
                        stale = !member_world ||
                                Space_IndexFromLattice(&member_world->grid_space.space,
                                                       member->as.cell.lattice_x, member->as.cell.lattice_y) < 0;
                    }
                    break;
                default:
                    stale = true; // A TARGET_KIND_NONE member should never be stored; drop it.
                    break;
            }
        }
        else
        {
            stale = true;
        }

        if (stale)
        {
            LArray_RemoveAt(&G_UIState.selection_set, index);
            pruned = true;
        }
        else
        {
            index++;
        }
    }

    if (pruned)
    {
        // Keep the back-compat single-fields consistent with the pruned set.
        UIState_SyncPrimaryFieldsFromSet();
    }
}

Newtonoid2d *UIState_GetSelectedObject(void)
{
    // Always validate before exposing the pointer to callers.
    UIState_ValidateSelection();
    return G_UIState.selected_object;
}

EntityId UIState_GetSelectedObjectId(void)
{
    return G_UIState.selected_object_id;
}

void UIState_SetSelectedCell(Cell *cell, int cell_index)
{
    // Keep cell pointer and index updates atomic from the caller's perspective.
    UIState_SetSelection(G_UIState.selected_object, cell, cell_index);
}

void UIState_ClearSelectedCell(void)
{
    UIState_SetSelectedCell(NULL, -1);
}

// Resolve the currently-selected cell LIVE from the central SelectionSet's primary member each
// call. The clicked cell is already recorded as a TARGET_KIND_CELL ref (world_index + cell_index)
// by Target_ResolveAtPoint / CommitClickSelection, so this derives the selected cell from that
// single source of truth rather than from a cached Cell* field. Resolving by index every call is
// deliberate: cells live in a per-frame-reset DArray and the buffer can be rebuilt, so a retained
// Cell* could dangle; re-resolving here keeps reads and writes landing on the LIVE cell that the
// physics step uses. Returns NULL when the primary selection is not a cell or the ids no longer
// resolve to a live cell.
Cell *UIState_GetSelectedCell(void)
{
    return UIState_ResolveCellRef(SelectionSet_Primary());
}

// Resolve any CELL TargetRef to its live Cell, re-deriving it from the world and lattice
// coordinate each call. Returns NULL for non-cell refs and stale coordinates. Shared by the
// primary-selection getter and the bulk-edit walker.
Cell *UIState_ResolveCellRef(TargetRef ref)
{
    if (ref.kind != TARGET_KIND_CELL)
    {
        return NULL;
    }

    // Resolve the owning world (handles the UNIVERSE_ROOT_WORLD_INDEX sentinel via Universe_GetWorld).
    World2d *world = Universe_GetWorld(&G_Universe, ref.as.cell.world_index);
    if (!world)
    {
        return NULL;
    }

    // Resolve the stored lattice coordinate against the LIVE grid (NULL if outside or no buffer), so the
    // answer stays correct after the world has grown or its grid_origin has shifted.
    return Space_GetCellAtLattice(&world->grid_space.space, ref.as.cell.lattice_x, ref.as.cell.lattice_y);
}

// Report the live dense index of any CELL ref (-1 for a non-cell ref or an unresolved world). The index
// is re-derived from the lattice coordinate each call because it changes when the world grows on the
// left/top.
int UIState_GetCellIndexForRef(TargetRef ref)
{
    if (ref.kind != TARGET_KIND_CELL)
    {
        return -1;
    }
    World2d *world = Universe_GetWorld(&G_Universe, ref.as.cell.world_index);
    return world ? Space_IndexFromLattice(&world->grid_space.space, ref.as.cell.lattice_x, ref.as.cell.lattice_y) : -1;
}
// Report the selected cell's index from the SelectionSet primary, or -1 when the primary is not a
// cell. Mirrors UIState_GetSelectedCell's single-source-of-truth derivation.
int UIState_GetSelectedCellIndex(void)
{
    return UIState_GetCellIndexForRef(SelectionSet_Primary());
}
// Report how many entities any CELL ref currently lists in its world's occupancy store (the previous
// tick's mapping), or 0 when the ref is not a cell or does not resolve.
int UIState_GetCellOccupancyForRef(TargetRef ref)
{
    int index = UIState_GetCellIndexForRef(ref);
    World2d *world = (index >= 0) ? Universe_GetWorld(&G_Universe, ref.as.cell.world_index) : NULL;
    return world ? CellOccupancy_Count(&world->cell_occupancy, index) : 0;
}
// Report how many entities the selected cell currently lists in its world's occupancy store, or 0
// when the primary is not a cell or the cell does not resolve.
int UIState_GetSelectedCellOccupancy(void)
{
    return UIState_GetCellOccupancyForRef(SelectionSet_Primary());
}// Return the currently active tool (TOOL_NONE when none is active).
ActiveTool UIState_GetActiveTool(void)
{
    return g_active_tool;
}

// Set the active tool. Radio-exclusive: the assignment replaces the previous tool, so activating
// one tool deactivates every other by construction.
void UIState_SetActiveTool(ActiveTool tool)
{
    g_active_tool = tool;
}

// Query whether the given tool is the active one (drives the toolbar button pressed state).
bool UIState_IsToolActive(ActiveTool tool)
{
    return g_active_tool == tool;
}

// Return the active tile type for the cell-paint tool (always a valid palette index).
int UIState_GetActiveTileType(void)
{
    return g_active_tile_type;
}

// Set the active tile type, wrapping it into the palette range (0 .. TilePalette_TypeCount()-1) so
// the stored value is always a drawable index. The palette count comes from the renderer (single
// source of truth) via the exposed accessor and is never duplicated here; negative inputs wrap
// correctly and a degenerate count (< 1) falls back to 0.
void UIState_SetActiveTileType(int tile_type)
{
    int count = TilePalette_TypeCount();
    if (count < 1)
    {
        g_active_tile_type = 0;
        return;
    }
    int wrapped = tile_type % count;
    if (wrapped < 0)
    {
        wrapped += count;
    }
    g_active_tile_type = wrapped;
}

// Advance the active tile type to the next palette index, wrapping 0->1->..->(count-1)->0. Driven
// by the cycle toolbar button; returns the new active type.
int UIState_CycleActiveTileType(void)
{
    UIState_SetActiveTileType(g_active_tile_type + 1);
    return g_active_tile_type;
}

// Return the active prefab index for the spawn tool (always a valid catalogue index, or 0 when the
// catalogue is empty).
int UIState_GetActivePrefab(void)
{
    return g_active_prefab;
}

// Set the active prefab, wrapping it into the catalogue range (0 .. Catalogue_Count()-1) so the
// stored value is always a valid entry index. The count comes from the catalogue (single source of
// truth) and is never duplicated here; negative inputs wrap correctly and an empty catalogue
// (count < 1) falls back to 0. Mirrors UIState_SetActiveTileType.
void UIState_SetActivePrefab(int index)
{
    int count = Catalogue_Count();
    if (count < 1)
    {
        g_active_prefab = 0;
        return;
    }
    int wrapped = index % count;
    if (wrapped < 0)
    {
        wrapped += count;
    }
    g_active_prefab = wrapped;
}

// Advance the active prefab to the next catalogue index, wrapping back to 0. Driven by the cycle
// toolbar button; returns the new active prefab index.
int UIState_CycleActivePrefab(void)
{
    UIState_SetActivePrefab(g_active_prefab + 1);
    return g_active_prefab;
}

// Report whether the in-world geometry editor is currently active.
bool UIState_IsGeometryEditActive(void)
{
    return g_geometry_edit_active;
}

// Set the geometry-editor active state explicitly (independent of the active tool).
void UIState_SetGeometryEditActive(bool active)
{
    g_geometry_edit_active = active;
}

// Flip the geometry-editor active state; returns the new state. Driven by the toolbar toggle
// command (CMD_TOGGLE_GEOMETRY_EDIT).
bool UIState_ToggleGeometryEdit(void)
{
    g_geometry_edit_active = !g_geometry_edit_active;
    return g_geometry_edit_active;
}

void InitUI(void)
{
    InitUIPalettes();

    // Register XML element builders once before any panels are created
    UILoader_RegisterDefaultBuilders();

    // Init Global UI State
    UIState_SetSelection(NULL, NULL, -1);
    G_UIState.focused_element = NULL;
    InitUtilityPanel();
    InitWorldManagerSystem();
    InitStateManagerSystem();
    InitPopupMenu();
    InitInfoStrip();
    InitDetailStrip();
    InitToolbar();
}

// Tears down every UI tree, panel, transient input state and drag capture.
void DestroyUI(void)
{
    // Remove text/focus state before any elements are freed to avoid stale pointers.
    ResetUIInputState();

    // Drop any in-flight drag capture so freed elements are not referenced.
    DragInteraction_ResetContext(DRAG_CONTEXT_UI);
    DragInteraction_ResetContext(DRAG_CONTEXT_GAME);

    // Clear popup visibility and submenu state while its elements are still valid.
    HidePopupMenu();

    // Each panel owns and disposes its root, views, selectors, and cached pointers.
    DestroyWorldManagerSystem();
    DestroyUtilityPanel();
    DestroyPopupMenu();
    DestroyStateManagerSystem();
    DestroyInfoStrip();
    DestroyDetailStrip();
    DestroyToolbar();

    // Destroy the global element pool so the next allocation creates a clean pool.
    Pool *ui_element_pool = GetUIElementPool();
    if (ui_element_pool)
    {
        PoolDestroy(ui_element_pool);
        SetUIElementPool(NULL);
    }

}

// Rebuilds the complete UI from the same initialisation path used at startup.
void ResetUI(void)
{
    // Capture memory snapshots around each phase so reset-time growth can be attributed precisely.
    size_t bytes_before = GetCurrentMemoryAllocated();

    DestroyUI();

    size_t bytes_after_destroy = GetCurrentMemoryAllocated();
    printf("[UI] Reset destroy phase: %.2f kB -> %.2f kB (delta %+0.2f kB)\n",
           (double)bytes_before / 1024.0,
           (double)bytes_after_destroy / 1024.0,
           ((double)bytes_after_destroy - (double)bytes_before) / 1024.0);

    InitUI();

    size_t bytes_after_init = GetCurrentMemoryAllocated();
    printf("[UI] Reset init phase: %.2f kB -> %.2f kB (delta %+0.2f kB)\n",
           (double)bytes_after_destroy / 1024.0,
           (double)bytes_after_init / 1024.0,
           ((double)bytes_after_init - (double)bytes_after_destroy) / 1024.0);
}

InputRouteResult UpdateUISystem(const InputFrame *input)
{
    if (!input)
    {
        return INPUT_ROUTE_IGNORED;
    }

    // Read-only hover resolution for the info strip. This only reads the pointer position and
    // must run every frame irrespective of routing, so it sits ahead of the route-claim logic
    // and never consumes input.
    InfoStrip_UpdateHover(input);

    int mouse_x = (int)input->pointer_position.x;
    int mouse_y = (int)input->pointer_position.y;
    UIElement *popup_root = GetPopupMenuRoot();
    // The Entity Mgmt (state manager) panel now occupies the lpanel viewport slot, so clicks over
    // lpanel_viewport must count as UI (that is what lets its ViewSelector tabs and controls take
    // input). The old entity_panel_viewport slot is now an empty reserved band with no panel, so
    // it is NOT gated here (gating empty space as UI would wrongly swallow world clicks there).
    bool cursor_in_ui = ViewportRegion_ContainsPixel(&toolbar_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&lpanel_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&rpanel_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&utility_panel_viewport, input->pointer_position) ||
                        // Popup: treat the click as UI when the pointer is over ANY popup
                        // element (main column OR a submenu), not just the popup root's box.
                        // The root is a FILL element shifted to the click point, so its box
                        // shrinks away from submenus sitting at a +x local offset; hit-testing
                        // the whole subtree (same GetElementAt the input dispatch uses) keeps
                        // submenu clicks in UI while empty space still falls through to the game.
                        (IsPopupMenuVisible() && popup_root &&
                         GetElementAt(popup_root, (Vector2d){(float)mouse_x, (float)mouse_y}) != NULL);
    bool cursor_in_game_viewport = ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position);

    // Any click outside the UI should drop UI keyboard focus so world/game actions can proceed.
    if (!cursor_in_ui && (input->left_pressed || input->right_pressed))
    {
        ClearUIFocus();
    }

    if (cursor_in_game_viewport && input->right_pressed)
    {
        if (IsPopupMenuVisible())
        {
            HidePopupMenu();
        }
        else
        {
            Vector2d popup_position = ResolvePixelToWorldFrame(
                &G_Universe.root_world,
                (Vector2d){(float)mouse_x, (float)mouse_y});
            ShowPopupMenu(popup_position);
        }
        cursor_in_ui = true;
    }

    DragInteractionState *drag_ctx = DragInteraction_BeginContextFrame(DRAG_CONTEXT_UI, input);
    ProcessUIInput(input, cursor_in_ui);
    InputRouteResult result = INPUT_ROUTE_IGNORED;
    if (drag_ctx->has_capture && (input->left_down || input->left_released))
    {
        result = INPUT_ROUTE_CAPTURED;
    }
    else if (cursor_in_ui)
    {
        result = INPUT_ROUTE_BLOCKED;
    }
    DragInteraction_EndContextFrame(DRAG_CONTEXT_UI, input);
    UpdateGlobalUIState();
    return result;
}

void DrawUI()
{
    DrawWorldManagerSystem();
    DrawStateManagerSystem();
    DrawUtilityPanel();
    DrawPopupMenu();
    DrawInfoStrip();
    DrawDetailStrip();
    DrawToolbar();
}

void UpdateUISpace(UIElement *root_element, UIBox seed_box)
{
    if (!root_element)
    {
        return;
    }

    UI_LayoutSubtree(root_element, seed_box);
}

// Write telemetry strings for visible stats labels without assuming every label exists.
static void UpdateTelemetryStats(float fps, float frame_time_ms, float kib_allocated,
                                 int newtonoid_count)
{
    // Only update every 20 frames to avoid unnecessary string formatting churn.
    if (frame_counter.total_frames % 20 != 0)
    {
        return;
    }

    if (G_UIState.stats_fps_str)
    {
        UpdateString64(G_UIState.stats_fps_str->string, "%.1f", fps);
    }
    if (G_UIState.stats_mem_str)
    {
        UpdateString64(G_UIState.stats_mem_str->string, "%.1f", kib_allocated);
    }
    if (G_UIState.stats_polygs_str)
    {
        UpdateString64(G_UIState.stats_polygs_str->string, "%d", newtonoid_count);
    }
    if (G_UIState.stats_ftime_str)
    {
        UpdateString64(G_UIState.stats_ftime_str->string, "%.2f", frame_time_ms);
    }
}

void UpdateGlobalUIState()
{
    // UPDATE STATISTICS
    float fps = frame_counter.fps;
    float ftime = frame_counter.delta_time * 1000;
    float bytes = GetCurrentMemoryAllocated() / 1024.0f;
    int polygs = GetNewtonoidCount();

    UpdateTelemetryStats(fps, ftime, bytes, polygs);

    // DEBUG----
    if (frame_counter.total_frames % 900 == 0)
    {
        const char *fps_text = G_UIState.stats_fps_str ? G_UIState.stats_fps_str->string : "N/A";
        const char *ftime_text = G_UIState.stats_ftime_str ? G_UIState.stats_ftime_str->string : "N/A";
        const char *mem_text = G_UIState.stats_mem_str ? G_UIState.stats_mem_str->string : "N/A";
        const char *poly_text = G_UIState.stats_polygs_str ? G_UIState.stats_polygs_str->string : "N/A";

        LOG_INFO("[Telemetry Update] FPS: %s  | F.TIME: %s | MEM: %sKB | POLY: %s\n",
                 fps_text, ftime_text, mem_text, poly_text);
    }
    // ----DEBUG //
}


