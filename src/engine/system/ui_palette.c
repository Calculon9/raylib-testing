/**********************************************************************************************
 *
 *   UI palette module: the built-in palettes, universal colour roles and surface styles.
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "common/common.h"
#include "ui/ui.h"
#include "system/ui_palette.h"
#include "system/ui_system.h"   // ResetUI (rebuild after a palette switch)

// Border thickness every themed surface role starts with (1 screen pixel, the historic look).
#define UI_DEFAULT_BORDER_WEIGHT 1

// Palette colour literal for the static initialisers below: a brace initialiser (not a compound
// literal), so it is a constant expression and the palettes can be `static const` data.
#define UI_RGBA(r, g, b, a) {(r), (g), (b), (a)}

// World colours shared by every palette. The default cell fill (parchment) and the cell line (olive,
// translucent) are the colours the world has always drawn, now owned by the palette and decoupled
// from input_fill. The world backgrounds, grid lines and axes differ per palette (see below).
#define UI_WORLD_CELL_FILL UI_RGBA(232, 224, 198, 255)
#define UI_WORLD_CELL_LINE UI_RGBA(96, 108, 56, 150)
// World background = panel_background x 0.65 (about 35% darker, same hue). The grid line and axis
// colours are the old olive/coral/teal lines pre-compensated for the darker background: they are
// drawn with alpha 150 (grid) and 220 (axes), so each value is the old colour plus (1 - alpha) / alpha
// times (panel_background - world_background). That makes the blended pixels identical to before.

// The four built-in palettes are constant designated initialisers: every field is named, so a
// palette that forgets a role is easy to spot, and no run-time assignment code is needed. Common
// to all of them: translucent dark dashboard backing, a faint white UI-borders overlay outline, and
// the same error and warning colours. Fields that look alike but differ in meaning (header_text,
// text_on_dark, muted_text, ...) stay separate. `text` is the one dark ink for body text, field
// labels and section titles; `header_text` is separate because panel headers are black.

const UIPalette ui_classic_palette = {
    .panel_background = UI_RGBA(32, 42, 37, 255),
    .world_background = UI_RGBA(21, 27, 24, 255),
    .world_cell_fill = UI_WORLD_CELL_FILL,
    .world_cell_line = UI_WORLD_CELL_LINE,
    .world_grid_line = UI_RGBA(104, 119, 65, 150),
    .world_axis_x = UI_RGBA(220, 119, 98, 220),
    .world_axis_y = UI_RGBA(88, 141, 129, 220),
    .container_border = UI_RGBA(194, 105, 83, 255),
    .container_fill = UI_RGBA(111, 137, 119, 255),
    .input_border = UI_RGBA(32, 42, 37, 255),
    .input_fill = UI_RGBA(245, 241, 230, 255),
    .button_border = UI_RGBA(32, 42, 37, 255),
    .button_fill = UI_RGBA(230, 194, 119, 255),
    .container_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .input_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .button_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    // Danger fill: the palette's terracotta container border.
    .button_danger_fill = UI_RGBA(194, 105, 83, 255),
    .text = UI_RGBA(10, 16, 12, 255),
    // Headers keep dark ink on a lighter container-hued fill; read-only fields use a dimmed input fill.
    .header_text = UI_RGBA(0, 0, 0, 255),
    .header_fill = UI_RGBA(214, 236, 224, 255),
    .readonly_fill = UI_RGBA(213, 207, 188, 255),
    .readonly_border = UI_RGBA(110, 128, 118, 255),
    .inactive_fill = UI_RGBA(78, 100, 86, 255),
    // Muted text (x/y prefixes, unit suffix): brown on the cream input, independent of the control roles.
    .muted_text = UI_RGBA(122, 100, 55, 255),
    .text_on_dark = UI_RGBA(245, 241, 230, 255),
    .error = UI_RGBA(230, 41, 55, 255),
    .warning = UI_RGBA(255, 255, 176, 255),
    .overlay_fill = UI_RGBA(8, 12, 18, 215),
    .debug_outline = UI_RGBA(255, 255, 255, 80),
};

const UIPalette ui_earth_palette = {
    .panel_background = UI_RGBA(38, 101, 115, 255),
    .world_background = UI_RGBA(25, 66, 75, 255),
    .world_cell_fill = UI_WORLD_CELL_FILL,
    .world_cell_line = UI_WORLD_CELL_LINE,
    .world_grid_line = UI_RGBA(105, 133, 84, 150),
    .world_axis_x = UI_RGBA(220, 123, 102, 220),
    .world_axis_y = UI_RGBA(88, 145, 133, 220),
    .container_border = UI_RGBA(191, 78, 36, 255),
    .container_fill = UI_RGBA(121, 120, 49, 255),
    .input_border = UI_RGBA(38, 101, 115, 255),
    .input_fill = UI_RGBA(242, 211, 172, 255),
    .button_border = UI_RGBA(38, 101, 115, 255),
    .button_fill = UI_RGBA(166, 109, 60, 255),
    .container_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .input_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .button_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    // Danger fill: the palette's rust container border.
    .button_danger_fill = UI_RGBA(214, 96, 52, 255),
    // Black is the only ink that passes on the olive container.
    .text = UI_RGBA(0, 0, 0, 255),
    .header_text = UI_RGBA(0, 0, 0, 255),
    .header_fill = UI_RGBA(196, 190, 120, 255),
    .readonly_fill = UI_RGBA(205, 183, 150, 255),
    .readonly_border = UI_RGBA(110, 112, 60, 255),
    .inactive_fill = UI_RGBA(90, 89, 36, 255),
    .muted_text = UI_RGBA(112, 74, 41, 255),
    .text_on_dark = UI_RGBA(242, 211, 172, 255),
    .error = UI_RGBA(230, 41, 55, 255),
    .warning = UI_RGBA(255, 255, 176, 255),
    .overlay_fill = UI_RGBA(8, 12, 18, 215),
    .debug_outline = UI_RGBA(255, 255, 255, 80),
};

const UIPalette ui_harbor_palette = {
    .panel_background = UI_RGBA(28, 58, 67, 255),
    // Recessed world viewport (#12262C), deeper than the panel chrome so the play space reads apart.
    .world_background = UI_RGBA(18, 38, 44, 255),
    .world_cell_fill = UI_WORLD_CELL_FILL,
    .world_cell_line = UI_WORLD_CELL_LINE,
    .world_grid_line = UI_RGBA(103, 122, 72, 150),
    .world_axis_x = UI_RGBA(220, 120, 100, 220),
    .world_axis_y = UI_RGBA(88, 142, 131, 220),
    .container_border = UI_RGBA(218, 117, 96, 255),
    // Lighter sage section fill (#78AA9E); the dark body ink below reaches 7.1:1 on it.
    .container_fill = UI_RGBA(120, 170, 158, 255),
    .input_border = UI_RGBA(28, 58, 67, 255),
    .input_fill = UI_RGBA(239, 221, 184, 255),
    .button_border = UI_RGBA(28, 58, 67, 255),
    .button_fill = UI_RGBA(220, 158, 70, 255),
    .container_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .input_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .button_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    // Danger fill: the mock's coral (equal to the container border).
    .button_danger_fill = UI_RGBA(218, 117, 96, 255),
    // One dark ink (#0A1418, near-black dark teal) for body text, field labels and section titles:
    // 7.1:1 on the #78AA9E container fill and 13.9:1 on the cream input fill.
    .text = UI_RGBA(10, 20, 24, 255),
    // Panel headers: black ink on a lighter sage (#9CC4B9) fill (amber in multi-select).
    .header_text = UI_RGBA(0, 0, 0, 255),
    .header_fill = UI_RGBA(156, 196, 185, 255),
    // Read-only fields: dimmed cream (#CFC5A8) with a muted sage border (#6E8F86); ink stays #0A1418.
    .readonly_fill = UI_RGBA(207, 197, 168, 255),
    .readonly_border = UI_RGBA(110, 143, 134, 255),
    // INACTIVE and CARD fill: darker than container_fill; light ink (text_on_dark) reaches 4.84:1 on it.
    .inactive_fill = UI_RGBA(58, 102, 94, 255),
    .muted_text = UI_RGBA(124, 87, 33, 255),
    .text_on_dark = UI_RGBA(239, 221, 184, 255),
    .error = UI_RGBA(230, 41, 55, 255),
    .warning = UI_RGBA(255, 255, 176, 255),
    .overlay_fill = UI_RGBA(8, 12, 18, 215),
    .debug_outline = UI_RGBA(255, 255, 255, 80),
};

const UIPalette ui_meadow_palette = {
    .panel_background = UI_RGBA(121, 120, 49, 255),
    .world_background = UI_RGBA(79, 78, 32, 255),
    .world_cell_fill = UI_WORLD_CELL_FILL,
    .world_cell_line = UI_WORLD_CELL_LINE,
    .world_grid_line = UI_RGBA(125, 137, 68, 150),
    .world_axis_x = UI_RGBA(225, 124, 99, 220),
    .world_axis_y = UI_RGBA(93, 146, 130, 220),
    .container_border = UI_RGBA(230, 194, 119, 255),
    .container_fill = UI_RGBA(111, 137, 119, 255),
    .input_border = UI_RGBA(38, 101, 115, 255),
    .input_fill = UI_RGBA(245, 241, 230, 255),
    .button_border = UI_RGBA(38, 101, 115, 255),
    .button_fill = UI_RGBA(230, 194, 119, 255),
    .container_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .input_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    .button_border_weight = UI_DEFAULT_BORDER_WEIGHT,
    // Danger fill: a dedicated terracotta, because meadow's container border is the button amber.
    .button_danger_fill = UI_RGBA(196, 92, 70, 255),
    .text = UI_RGBA(10, 16, 12, 255),
    .header_text = UI_RGBA(0, 0, 0, 255),
    .header_fill = UI_RGBA(190, 214, 196, 255),
    .readonly_fill = UI_RGBA(213, 207, 188, 255),
    .readonly_border = UI_RGBA(96, 140, 130, 255),
    .inactive_fill = UI_RGBA(78, 100, 86, 255),
    .muted_text = UI_RGBA(122, 100, 55, 255),
    .text_on_dark = UI_RGBA(245, 241, 230, 255),
    .error = UI_RGBA(230, 41, 55, 255),
    .warning = UI_RGBA(255, 255, 176, 255),
    .overlay_fill = UI_RGBA(8, 12, 18, 215),
    .debug_outline = UI_RGBA(255, 255, 255, 80),
};
#undef UI_RGBA
#undef UI_WORLD_CELL_FILL
#undef UI_WORLD_CELL_LINE

// The live palette storage every UI element is built from. Starts as Harbor (copied on first use by
// InitUIPalettes) and is overwritten by UIPalette_SetActive; its address never changes.
UIPalette ui_default_palette = {0};

static bool ui_palettes_initialized = false;

// Resolve the world colour group (background, default cell fill and line, grid and axes) from a
// palette, so the world renderer never reads panel chrome colours or hard-codes its own. A NULL
// palette uses the live one.
UIWorldColours UIPalette_GetWorldColours(const UIPalette *palette)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }
    return (UIWorldColours){
        .background = palette->world_background,
        .cell_fill = palette->world_cell_fill,
        .cell_line = palette->world_cell_line,
        .grid_line = palette->world_grid_line,
        .axis_x = palette->world_axis_x,
        .axis_y = palette->world_axis_y,
    };
}

// Seed the live palette with the Harbor default on first use. Idempotent.
static void InitUIPalettes(void)
{
    if (ui_palettes_initialized)
    {
        return;
    }
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
            colours.fill = palette->inactive_fill;
            colours.text = palette->text_on_dark;
            colours.border = palette->input_border;
            break;
        case UI_ROLE_CARD:
            // Grouped-field card: dark teal with cream text. Today it shares inactive_fill; it stays
            // a distinct role so a card_fill palette field can be added back to diverge later.
            colours.fill = palette->inactive_fill;
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
    [UI_PALETTE_SURFACE_INPUT]       = {true,  UI_ROLE_INPUT,    SURFACE_WEIGHT_INPUT,     false},
    [UI_PALETTE_SURFACE_BUTTON]      = {true,  UI_ROLE_ACTIVE,   SURFACE_WEIGHT_BUTTON,    true},
    [UI_PALETTE_SURFACE_PANEL]       = {false, UI_ROLE_SHEET,    SURFACE_WEIGHT_NONE,      false},
    [UI_PALETTE_SURFACE_DANGER]      = {true,  UI_ROLE_DANGER,   SURFACE_WEIGHT_BUTTON,    true},  // danger shares the button border
    [UI_PALETTE_SURFACE_INACTIVE]    = {true,  UI_ROLE_INACTIVE, SURFACE_WEIGHT_INPUT,     true},  // inactive tiles share the input border
    [UI_PALETTE_SURFACE_CARD]        = {true,  UI_ROLE_CARD,     SURFACE_WEIGHT_INPUT,     true},  // cards share the input border
};

// Resolve a surface= name to its fill, border, border weight and paired text colour from the
// s_surface_map table. Role-backed surfaces take fill and border (and text, where the row says so)
// from UIPalette_GetRoleColours so the pairings live in one place. The one surface with no role,
// panel, reads the palette's dark background (no border). Unknown surfaces resolve to the
// transparent style. A NULL palette uses the default.
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
    else if (surface == UI_PALETTE_SURFACE_PANEL)
    {
        style.fill = palette->panel_background;
    }

    switch (row->weight)
    {
        case SURFACE_WEIGHT_CONTAINER: style.border_weight = palette->container_border_weight; break;
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

