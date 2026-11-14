/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H
#include "common/common.h"
#include "input/drag_interaction.h"
#include "input/pointer_input.h"
#include "math/cvectors.h"
#include "ui/ui.h"
#include "camera/camera.h"
#include "world/target_resolver.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef enum
{
	UI_PALETTE_SURFACE_TRANSPARENT,
	UI_PALETTE_SURFACE_CONTAINER,
	UI_PALETTE_SURFACE_FIELD_ROW,
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
	ColourRgba field_row_border;
	ColourRgba field_row_fill;
	ColourRgba input_border;
	ColourRgba input_fill;
	ColourRgba button_border;
	ColourRgba button_fill;
	// Border line thickness (screen pixels) per surface role. panel/transparent have no border.
	uint8_t container_border_weight;
	uint8_t field_row_border_weight;
	uint8_t input_border_weight;
	uint8_t button_border_weight;
	ColourRgba button_danger_fill; // Fill for destructive buttons such as DELETE (dark ink on top).
	ColourRgba text;
	ColourRgba label_text;       // Field labels on container_fill (dark, AA contrast).
	ColourRgba title_text;       // Section title ink (drawn on container_fill beside the accent bar).
	ColourRgba header_text;      // Dark ink for panel header title lines (entity/world headers on header_fill or amber).
	ColourRgba header_fill;      // Panel header box fill (lighter than container_fill; amber button_fill in multi-select).
	ColourRgba readonly_fill;    // Fill of every read-only text field (dimmed input_fill).
	ColourRgba readonly_border;  // 1 px border of every read-only text field.
	ColourRgba subsection_fill;  // INACTIVE control role fill (dark teal): OFF toggles, inactive tabs (text_on_dark on top).
	ColourRgba card_fill;        // CARD role fill (dark teal): grouped-field cards (text_on_dark on top). Kept apart from subsection_fill so the roles can diverge.
	ColourRgba muted_text;       // Muted brown for x/y prefixes and unit suffixes on input fills (not a control role).
	ColourRgba text_on_dark;
	ColourRgba error;
	ColourRgba warning;
	ColourRgba overlay_fill;     // Translucent dark backing for the diagnostics dashboard.
	ColourRgba debug_outline;    // Outline drawn around every element by the UI-borders debug overlay.
} UIPalette;

// Active tool for the vertical tool toolbar. Tools are radio-exclusive: at most one is active at
// a time. TOOL_NONE is the default (no additive tool action runs). A tool is purely additive and
// never suppresses the normal selection/drag/marquee input; its action is hung off the
// confirmed-click commit site so a genuine drag never sprays the action.
typedef enum ActiveTool
{
	TOOL_NONE = 0,   // No tool active; only normal selection/drag/marquee behaviour runs.
	TOOL_NUDGE,      // Nudge tool: a confirmed click adds a random-direction linear impulse.
	TOOL_CELL_PAINT, // Cell paint tool: a confirmed click marks the cell under the pointer PRESENT.
	TOOL_SPAWN,      // Spawn tool: a confirmed click places the active prefab into the root world.
} ActiveTool;

// Forward declarations keep selection APIs lightweight without pulling full physics/world headers.
typedef struct Newtonoid2d Newtonoid2d;
typedef struct Cell Cell;
typedef void (*UIStateSelectionChangedCallback)(EntityId selected_object_id,
												const Newtonoid2d *selected_object,
												const Cell *selected_cell,
												int selected_cell_index,
												void *user_data);



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// Default UI Properties
extern const Size ui_standard_control_size;
extern const Size ui_standard_textbox_size;
extern const Size ui_standard_textfield_input_size;
extern Vector2d tfield_default_padding;// = {0.04, 0.04};
extern const Size ui_standard_container_size;
extern const Size ui_fill_container_size;
extern const Vector2d ui_standard_container_padding;
extern const Vector2d ui_standard_field_padding;
// Padding inside a boxed section (6 UI px).
extern const Vector2d ui_section_padding;
// ViewHost / View padding added on top of the root padding (root 5 + 1 = 6 px panel inset).
extern const Vector2d ui_host_padding;
// Root padding for the read-only info / detail strips (5 px horizontal inset, no vertical).
extern const Vector2d ui_strip_host_padding;
// Fallback text-field label column width, used until the per-section column pass has run.
extern const float ui_standard_textfield_label_width;
extern const Size ui_standard_button_size;
// Icon-only tool/toggle button size (34 x 25 UI px), used by the toolbar.
extern const Size ui_standard_icon_button_size;
// Container title label size: width hugs the measured text, height stays fixed at 0.5.
extern const Size ui_title_size;
extern const Size ui_wide_button_size;
extern const Size ui_small_horizontal_button_size;
extern const Size ui_fill_button_size;
extern const Vector2d ui_standard_button_padding;
// Padding of tabs and tab-like options (they hug their text plus this on both sides).
extern const Vector2d ui_standard_tab_padding;
extern const Size ui_standard_selector_container_size;
extern const Size ui_standard_selector_button_size;
extern const Layout ui_standard_stack_spacing;
// Stacked spacing between a panel's top-level blocks (header, tabs, sections): 6 px.
extern const Layout ui_view_stack_spacing;
extern const Layout ui_compact_stack_spacing;
extern const Layout ui_standard_stack_wrap_spacing;
extern const Layout ui_standard_stack_smart_wrap_spacing;
extern const Layout ui_compact_stack_wrap_spacing;
extern const Layout ui_zero_x_stack_wrap_spacing;
extern const Layout ui_standard_inline_wrap_spacing;
extern const Layout ui_zero_x_inline_wrap_spacing;
extern const Layout ui_standard_inline_spacing;
extern const Layout ui_zero_inline_spacing;

extern UIPalette ui_default_palette;
extern UIPalette ui_classic_palette;
extern UIPalette ui_earth_palette;
extern UIPalette ui_harbor_palette;
extern UIPalette ui_meadow_palette;

// The palette every UI element is built from. Returns a stable pointer to the live storage
// (ui_default_palette), so the pointer stays valid across UIPalette_SetActive calls. UI code
// outside ui_system.c must obtain its palette through this (or its host's palette), never by
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
	UI_ROLE_CARD,           // Grouped-field card on the sheet: dark teal fill with cream text.
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
// Paired text colour for surfaces that are control roles (button, danger, inactive, card); false for
// surfaces with no text role. Lets the loader give a surface its text colour automatically.
bool UIPalette_GetSurfaceText(const UIPalette *palette, UIPaletteSurface surface, ColourRgba *out_text);
// Resolves a role for a control drawn in a given context. A control that would be INACTIVE inside a
// CARD resolves to the SHEET fill with ink text (keeping the INACTIVE border), so OFF chips stay
// visible against the teal card. Every other role and context resolves as UIPalette_GetRoleColours.
UIRoleColours UIPalette_GetRoleColoursInContext(const UIPalette *palette, UIColourRole role, bool inside_card);
// Border thickness (screen pixels) for a surface role from the palette; 0 for borderless roles.
uint8_t UIPalette_GetSurfaceBorderWeight(const UIPalette *palette, UIPaletteSurface surface);
void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
								 ColourRgba *out_border, ColourRgba *out_fill);
// Two-state colours shared by tabs, kind chips and flag toggles: the ACTIVE or INACTIVE role's
// fill and text. A NULL palette uses the default; NULL outputs are skipped.
void UIPalette_GetToggleColours(const UIPalette *palette, bool active,
								ColourRgba *out_fill, ColourRgba *out_ink);
// Sets an element's fill (and border, where the role has one) and, for buttons and labels, its
// font colour from a colour role, so a region never pairs colours by hand. NULL element or
// palette is a no-op. An INACTIVE role on an element inside a card uses the sheet colours
// (see UIPalette_GetRoleColoursInContext).
void UIElement_ApplyRoleColours(UIElement *element, const UIPalette *palette, UIColourRole role);
InputRouteResult UpdateUISystem(const InputFrame *input);

// Centralised selection setters reduce cross-module direct writes to UI state.
void UIState_SetSelectedObject(Newtonoid2d *object);
void UIState_SetSelectedObjectById(EntityId entity_id);
void UIState_SetSelection(Newtonoid2d *object, Cell *cell, int cell_index);
void UIState_SetSelectionChangedCallback(UIStateSelectionChangedCallback callback, void *user_data);
void UIState_ClearSelectedObject(void);
void UIState_ValidateSelection(void);
// Prune stale (deleted/out-of-range) members from the central selection set; call once per frame.
void UIState_ValidateSelectionSet(void);
Newtonoid2d *UIState_GetSelectedObject(void);
EntityId UIState_GetSelectedObjectId(void);
void UIState_SetSelectedCell(Cell *cell, int cell_index);
void UIState_ClearSelectedCell(void);
Cell *UIState_GetSelectedCell(void);
int UIState_GetSelectedCellIndex(void);
int UIState_GetCellIndexForRef(TargetRef ref);     // Live dense index of any CELL ref (-1 if not a cell / unresolved).
int UIState_GetCellOccupancyForRef(TargetRef ref); // Occupancy count of any CELL ref (0 if not a cell / unresolved).
int UIState_GetSelectedCellOccupancy(void); // Entities listed in the selected cell's occupancy store entry (0 if none).

// Central mixed SelectionSet operations. The set (TargetRef entries owned in G_UIState) is the
// source of truth for multi-selection; the single-valued getters above mirror its primary member.
void SelectionSet_Clear(void);                 // Empty the set (retaining capacity).
bool SelectionSet_Contains(TargetRef ref);     // True when an equal ref is already present.
int SelectionSet_Count(void);                  // Number of members currently selected.
void SelectionSet_Add(TargetRef ref);          // Add a member (idempotent; ignores TARGET_KIND_NONE).
bool SelectionSet_Remove(TargetRef ref);       // Remove an equal member; returns whether removed.
void SelectionSet_Toggle(TargetRef ref);       // Remove if present, otherwise add.
bool SelectionSet_Get(int index, TargetRef *out); // Member at index; false if out of range (no allocation).
Cell *UIState_ResolveCellRef(TargetRef ref);   // Live Cell* for a CELL ref, or NULL (stale / wrong kind).
TargetRef SelectionSet_Primary(void);          // Last-added member, or a TARGET_KIND_NONE ref if empty.

// Re-derive the single-valued primary fields from the set's primary member, firing the
// selection-changed callback when they actually change. Called after set toggles/pruning so the
// back-compat getters stay consistent with the set.
void UIState_SyncPrimaryFieldsFromSet(void);

// Context world: the single "current world", derived on every call from SelectionSet_Primary() (no
// stored copy). World primary -> that world; entity/cell primary -> its owning world; empty set or
// stale primary -> root. The index is a nested index (0..world_count-1) or UNIVERSE_ROOT_WORLD_INDEX,
// never -1; test `index >= 0` to tell a nested world from root.
int UIState_GetContextWorldIndex(void);
// The context world itself; never NULL (the root world when the index is UNIVERSE_ROOT_WORLD_INDEX).
World2d *UIState_GetContextWorld(void);

// Active-tool state for the tool toolbar (radio-exclusive). The toolbar buttons query the active
// tool for their pressed/unpressed visual, and setting a tool replaces the active tool.
// Return the currently active tool (TOOL_NONE when no tool is active).
ActiveTool UIState_GetActiveTool(void);
// Set the active tool. Radio-exclusive: assigning a tool replaces the previous one, which
// deactivates every other tool by construction.
void UIState_SetActiveTool(ActiveTool tool);
// Query whether a specific tool is the active one (used by toolbar button bindings).
bool UIState_IsToolActive(ActiveTool tool);

// Active tile type for the cell-paint tool (module-owned, like the active tool). The paint-commit
// reads this to stamp painted cells; the cycle toolbar button advances it. The stored value is
// always clamped/wrapped to the shared tile palette range (0 .. TilePalette_TypeCount()-1).
// Return the current active tile type.
int  UIState_GetActiveTileType(void);
// Set the active tile type, clamped/wrapped into the palette range so it is always a valid index.
void UIState_SetActiveTileType(int tile_type);
// Advance the active tile type to the next palette index (wraps back to 0); returns the new value.
int  UIState_CycleActiveTileType(void);

// Active prefab for the spawn tool (module-owned, like the active tile type). The spawn commit uses
// this to choose the catalogue entry; the cycle toolbar button advances it. The stored value is
// always clamped/wrapped to the catalogue range (0 .. Catalogue_Count()-1).
// Return the current active prefab index.
int  UIState_GetActivePrefab(void);
// Set the active prefab, clamped/wrapped into the catalogue range so it is always a valid index
// (or 0 when the catalogue is empty).
void UIState_SetActivePrefab(int index);
// Advance the active prefab to the next catalogue index (wraps back to 0); returns the new value.
int  UIState_CycleActivePrefab(void);

// Independent on/off state for the in-world geometry (vertex) editor (module-owned, like the
// active tool). It is NOT radio-exclusive with the tools and is NOT a debug overlay; the geometry
// editor's single enable gate reads it together with a non-NULL selection.
// Report whether the geometry editor is currently active.
bool UIState_IsGeometryEditActive(void);
// Set the geometry-editor active state explicitly (independent of the active tool).
void UIState_SetGeometryEditActive(bool active);
// Flip the geometry-editor active state (driven by the toolbar toggle); returns the new state.
bool UIState_ToggleGeometryEdit(void);
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);
void UpdateUISpace(UIElement *root_element, UIBox seed_box);
void DestroyUI(void);
void ResetUI(void);

#endif
