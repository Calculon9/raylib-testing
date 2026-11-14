/**********************************************************************************************
*
UI PALETTE MODULE
*
* The colour palettes, universal colour roles and the surface styles built from them. Split out of
* ui_system.h so code that only needs colours does not pull in input, camera and world headers.
*
**********************************************************************************************/
#ifndef UI_PALETTE_H
#define UI_PALETTE_H
#include "common/common.h"
#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
	UI_PALETTE_SURFACE_TRANSPARENT,
	UI_PALETTE_SURFACE_CONTAINER,
	UI_PALETTE_SURFACE_INPUT,
	UI_PALETTE_SURFACE_BUTTON,
	UI_PALETTE_SURFACE_PANEL,  // Dark panel background (ViewHost / View), no border.
	UI_PALETTE_SURFACE_DANGER,  // Destructive button: button border with the coral danger fill.
	UI_PALETTE_SURFACE_INACTIVE,  // INACTIVE control role (OFF tiles, inactive tabs): dark teal fill, input border.
	UI_PALETTE_SURFACE_CARD       // CARD role: grouped-field card on the sheet (dark teal fill, input border, cream text).
} UIPaletteSurface;

typedef struct UIPalette
{
	ColourRgba panel_background;
	ColourRgba container_border;
	ColourRgba container_fill;
	ColourRgba input_border;
	ColourRgba input_fill;
	ColourRgba button_border;
	ColourRgba button_fill;
	// Border line thickness (screen pixels) per surface role. panel/transparent have no border.
	uint8_t container_border_weight;
	uint8_t input_border_weight;
	uint8_t button_border_weight;
	ColourRgba button_danger_fill; // Fill for destructive buttons such as DELETE (dark ink on top).
	ColourRgba text;             // The one dark ink: body text, field labels and section titles on sheets and inputs.
	ColourRgba header_text;      // Dark ink for panel header title lines (entity/world headers on header_fill or amber).
	ColourRgba header_fill;      // Panel header box fill (lighter than container_fill; amber button_fill in multi-select).
	ColourRgba readonly_fill;    // Fill of every read-only text field (dimmed input_fill).
	ColourRgba readonly_border;  // 1 px border of every read-only text field.
	ColourRgba inactive_fill;    // INACTIVE and CARD role fill (dark teal): OFF toggles, inactive tabs, grouped-field cards (text_on_dark on top).
	ColourRgba muted_text;       // Muted brown for x/y prefixes and unit suffixes on input fills (not a control role).
	ColourRgba text_on_dark;
	ColourRgba error;
	ColourRgba warning;
	ColourRgba overlay_fill;     // Translucent dark backing for the diagnostics dashboard.
	ColourRgba debug_outline;    // Outline drawn around every element by the UI-borders debug overlay.
	// Game world colours (not UI chrome). Read through UIPalette_GetWorldColours.
	ColourRgba world_background; // World viewport clear: a recessed (about 35% darker) shade of panel_background.
	ColourRgba world_cell_fill;  // Default cell fill; a per-cell colour or tile overrides it later.
	ColourRgba world_cell_line;  // Cell grid lines and the bold world outline drawn over the cells.
	ColourRgba world_grid_line;  // Universe grid lines over world_background (tuned to that background).
	ColourRgba world_axis_x;     // Universe X axis line.
	ColourRgba world_axis_y;     // Universe Y axis line.
} UIPalette;

// The world colour role group: everything the game world draws, resolved from one palette.
typedef struct UIWorldColours
{
	ColourRgba background; // Viewport clear colour behind the cells (never panel_background).
	ColourRgba cell_fill;  // Default cell fill, used when a cell has no colour or tile of its own.
	ColourRgba cell_line;  // Cell grid lines and world border.
	ColourRgba grid_line;  // Universe grid lines.
	ColourRgba axis_x;     // Universe X axis.
	ColourRgba axis_y;     // Universe Y axis.
} UIWorldColours;

// Resolves the world colour group from a palette. A NULL palette uses the active one.
UIWorldColours UIPalette_GetWorldColours(const UIPalette *palette);

//----------------------------------------------------------------------------------
// Palettes, roles and surface styles
//----------------------------------------------------------------------------------

extern UIPalette ui_default_palette;
extern const UIPalette ui_classic_palette;
extern const UIPalette ui_earth_palette;
extern const UIPalette ui_harbor_palette;
extern const UIPalette ui_meadow_palette;

// The palette every UI element is built from. Returns a stable pointer to the live storage
// (ui_default_palette), so the pointer stays valid across UIPalette_SetActive calls. UI code
// outside ui_palette.c must obtain its palette through this (or its host's palette), never by
// reading ui_default_palette directly.
const UIPalette *UIPalette_GetActive(void);
// Switches the live UI to `src` (copied by value, so any palette, e.g. &ui_earth_palette, can be
// passed) and rebuilds the UI so every element picks up the new colours. NULL is ignored. Before
// InitUI has run the palette is only stored. Element colours are copied at construction, so the
// rebuild resets transient UI state. Call from update/input code (as the debug overlay hotkeys call
// ResetUI), never from inside a UI event handler or during the UI draw traversal.
void UIPalette_SetActive(const UIPalette *src);

// Universal colour roles: each pairs a fill with the text colour drawn on it, plus a border.
// Views and elements pick a role and never choose their own fill or text colours.
typedef enum
{
	UI_ROLE_SHEET,          // Sage sheet (container_fill) with ink text.
	UI_ROLE_HEADER,         // Panel header fill with black header text.
	UI_ROLE_INPUT,          // Editable field: cream fill, input border, ink text.
	UI_ROLE_READONLY_INPUT, // Read-only field: dimmed cream, muted border, ink text.
	UI_ROLE_INACTIVE,       // Control off-state: dark teal fill with cream text.
	UI_ROLE_CARD,           // Grouped-field card on the sheet: dark teal fill with cream text (reads inactive_fill today; a distinct role so it can diverge by adding a palette field).
	UI_ROLE_ACTIVE,         // Control on-state / default button: amber fill with ink text.
	UI_ROLE_DANGER          // Destructive control: coral fill with ink text.
} UIColourRole;

typedef struct UIRoleColours
{
	ColourRgba fill;   // Surface fill.
	ColourRgba text;   // Text colour drawn on that fill.
	ColourRgba border; // Border colour (COLOURLESS_RGBA when the role has none).
} UIRoleColours;

// Resolves a role to its fill/text/border. A NULL palette uses the default.
UIRoleColours UIPalette_GetRoleColours(const UIPalette *palette, UIColourRole role);
// Everything a surface= name resolves to: fill, border colour, border thickness and, for surfaces
// that are control roles (button, danger, inactive, card), the paired text colour.
typedef struct UISurfaceStyle
{
ColourRgba fill;         // Surface fill (COLOURLESS_RGBA for transparent).
ColourRgba border;       // Border colour (COLOURLESS_RGBA when the surface has none).
uint8_t border_weight;   // Border thickness in UI pixels; 0 for borderless surfaces.
bool has_text;           // True when the surface carries a paired text colour.
ColourRgba text;         // Paired text colour (valid only when has_text).
} UISurfaceStyle;

// Resolves a surface to its style from one table, so constructors, the loader and panels never pair
// a fill with a border weight or text colour by hand. A NULL palette uses the default.
UISurfaceStyle UIPalette_GetSurfaceStyle(const UIPalette *palette, UIPaletteSurface surface);
// Sets an element's fill (and border, where the role has one) and, for buttons and labels, its
// font colour from a colour role, so a region never pairs colours by hand. NULL element or
// palette is a no-op. An INACTIVE role on an element flagged in_card resolves to the sheet fill with
// ink text (keeping the INACTIVE border), so OFF chips stay visible against the teal card.
void UIElement_ApplyRoleColours(UIElement *element, const UIPalette *palette, UIColourRole role);
// Flags every descendant of `root` (not `root` itself) as inside a card, so their INACTIVE role
// resolves as the sheet colours. Called once when a card subtree is built; costs nothing per frame.
void UIElement_MarkDescendantsInCard(UIElement *root);

#endif
