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
	UI_PALETTE_SURFACE_BUTTON
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
	ColourRgba toggle_on_fill;  // Fill for a source-bearing toggle button in its ON state.
	ColourRgba toggle_off_fill; // Fill for a source-bearing toggle button in its OFF state (same hue, dimmed).
	ColourRgba text;
	ColourRgba label_text;
	ColourRgba text_on_dark;
	ColourRgba error;
	ColourRgba warning;
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
extern const Size ui_standard_button_size;
extern const Size ui_wide_button_size;
extern const Size ui_small_horizontal_button_size;
extern const Size ui_fill_button_size;
extern const Vector2d ui_standard_button_padding;
extern const Size ui_standard_selector_container_size;
extern const Size ui_standard_selector_button_size;
extern const Layout ui_standard_stack_spacing;
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

void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
								 ColourRgba *out_border, ColourRgba *out_fill);
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

// Central mixed SelectionSet operations. The set (TargetRef entries owned in G_UIState) is the
// source of truth for multi-selection; the single-valued getters above mirror its primary member.
void SelectionSet_Clear(void);                 // Empty the set (retaining capacity).
bool SelectionSet_Contains(TargetRef ref);     // True when an equal ref is already present.
int SelectionSet_Count(void);                  // Number of members currently selected.
void SelectionSet_Add(TargetRef ref);          // Add a member (idempotent; ignores TARGET_KIND_NONE).
bool SelectionSet_Remove(TargetRef ref);       // Remove an equal member; returns whether removed.
void SelectionSet_Toggle(TargetRef ref);       // Remove if present, otherwise add.
TargetRef SelectionSet_Primary(void);          // Last-added member, or a TARGET_KIND_NONE ref if empty.

// Re-derive the single-valued primary fields from the set's primary member, firing the
// selection-changed callback when they actually change. Called after set toggles/pruning so the
// back-compat getters stay consistent with the set.
void UIState_SyncPrimaryFieldsFromSet(void);

// Active-tool state for the tool toolbar (radio-exclusive). The toolbar buttons query the active
// tool for their pressed/unpressed visual, and setting a tool replaces the active tool.
// Return the currently active tool (TOOL_NONE when no tool is active).
ActiveTool UIState_GetActiveTool(void);
// Set the active tool. Radio-exclusive: assigning a tool replaces the previous one, which
// deactivates every other tool by construction.
void UIState_SetActiveTool(ActiveTool tool);
// Query whether a specific tool is the active one (used by toolbar button bindings).
bool UIState_IsToolActive(ActiveTool tool);
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);
void UpdateUISpace(UIElement *root_element, UIBox seed_box);
void DestroyUI(void);
void ResetUI(void);

#endif
