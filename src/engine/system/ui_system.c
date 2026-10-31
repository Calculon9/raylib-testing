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
#include "system/ui/lpanel_system.h"
#include "system/ui/rpanel_system.h"
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
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
//Vector2d tfield_default_padding = {0.0, 0.0};
const Vector2d ui_standard_container_padding = {0.06, 0.06};
const Vector2d ui_standard_field_padding = {0.02f, 0.0f};
const Vector2d ui_standard_button_padding = {0.025, 0.025};
const Size ui_standard_control_size = {{4.65, 0.50}, AXIS_FIXED, AXIS_FIXED};
//const Size ui_standard_textbox_size = {{4.55f, 0.50f}, AXIS_FIXED, AXIS_FIXED};
// Give labels more room in inline text fields (label width = 1.0 - textbox width).
const Size ui_standard_textfield_input_size = {{0.62f, 1.0f}, AXIS_PERCENT, AXIS_PERCENT};
const Size ui_standard_button_size = {{3.22f, 0.5f}, AXIS_FIXED, AXIS_FIXED};
const Size ui_wide_button_size = {{4.65f, 0.5f}, AXIS_FIXED, AXIS_FIXED};
const Size ui_small_horizontal_button_size = {{1.91f, 0.5f}, AXIS_FIXED, AXIS_FIXED};
const Size ui_fill_button_size = {{1.0f, 1.0f}, AXIS_FILL, AXIS_FILL};
const Size ui_standard_container_size = {{1, 0.5}, AXIS_PERCENT, AXIS_PERCENT};
const Size ui_fill_container_size = {{1.0f, 1.0f}, AXIS_FILL, AXIS_FILL};
const Size ui_standard_selector_container_size = UI_SIZE_HUG;
const Size ui_standard_selector_button_size = {{1.75f, 0.5f}, AXIS_FIXED, AXIS_FIXED};
// NOTE: the middle field of each Layout literal is NumberForm (NONE), NOT an AxisSizeMode.
const Layout ui_standard_stack_spacing = {{0.0f, 0.06f}, NONE, FLOW_STACKED};
const Layout ui_compact_stack_spacing = {{0.0f, 0.025f}, NONE, FLOW_STACKED};
const Layout ui_standard_stack_wrap_spacing = {{0.06f, 0.06f}, NONE, FLOW_STACKED_WRAP};
const Layout ui_standard_stack_smart_wrap_spacing = {{0.06f, 0.06f}, NONE, FLOW_STACKED_SMART_WRAP};
const Layout ui_compact_stack_wrap_spacing = {{0.025f, 0.025f}, NONE, FLOW_STACKED_WRAP};
const Layout ui_zero_x_stack_wrap_spacing = {{0.0f, 0.06f}, NONE, FLOW_STACKED_WRAP};
const Layout ui_standard_inline_wrap_spacing = {{0.06f, 0.06f}, NONE, FLOW_INLINE_WRAP};
const Layout ui_zero_x_inline_wrap_spacing = {{0.0f, 0.06f}, NONE, FLOW_INLINE_WRAP};
const Layout ui_standard_inline_spacing = {{0.06f, 0.0f}, NONE, FLOW_INLINE};
const Layout ui_zero_inline_spacing = {{0.0f, 0.0f}, NONE, FLOW_INLINE};

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
    ui_classic_palette.button_fill = MakeColour(230, 194, 119, 255);
    // Toggle fills: ON reuses the amber button fill; OFF is the same hue dimmed towards the panel.
    ui_classic_palette.toggle_on_fill = MakeColour(230, 194, 119, 255);
    ui_classic_palette.toggle_off_fill = MakeColour(156, 132, 81, 255);
    ui_classic_palette.text = MakeColour(32, 42, 37, 255);
    ui_classic_palette.label_text = MakeColour(255, 255, 255, 255);
    ui_classic_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_classic_palette.error = MakeColour(230, 41, 55, 255);
    ui_classic_palette.warning = MakeColour(255, 255, 176, 255);

    ui_earth_palette.panel_background = MakeColour(38, 101, 115, 255);
    ui_earth_palette.container_border = MakeColour(191, 78, 36, 255);
    ui_earth_palette.container_fill = MakeColour(121, 120, 49, 255);
    ui_earth_palette.field_row_border = MakeColour(166, 109, 60, 255);
    ui_earth_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_earth_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.input_fill = MakeColour(242, 211, 172, 255);
    ui_earth_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.button_fill = MakeColour(166, 109, 60, 255);
    // Toggle fills: ON reuses the terracotta button fill; OFF is the same hue dimmed towards the panel.
    ui_earth_palette.toggle_on_fill = MakeColour(166, 109, 60, 255);
    ui_earth_palette.toggle_off_fill = MakeColour(112, 74, 41, 255);
    ui_earth_palette.text = MakeColour(38, 101, 115, 255);
    ui_earth_palette.label_text = MakeColour(166, 109, 60, 255);
    ui_earth_palette.text_on_dark = MakeColour(242, 211, 172, 255);
    ui_earth_palette.error = MakeColour(230, 41, 55, 255);
    ui_earth_palette.warning = MakeColour(255, 255, 176, 255);

    ui_harbor_palette.panel_background = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.container_border = MakeColour(218, 117, 96, 255);
    ui_harbor_palette.container_fill = MakeColour(86, 139, 127, 255);
    ui_harbor_palette.field_row_border = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_harbor_palette.input_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.input_fill = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.button_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.button_fill = MakeColour(220, 158, 70, 255);
    // Toggle fills: ON reuses the amber button fill; OFF is the same amber hue dimmed towards the panel.
    ui_harbor_palette.toggle_on_fill = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.toggle_off_fill = MakeColour(150, 108, 48, 255);
    // Contrast bump: near-black dark-teal ink (hue preserved) reads crisply on the sage container fill.
    ui_harbor_palette.text = MakeColour(18, 30, 34, 255);
    ui_harbor_palette.label_text = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.text_on_dark = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.error = MakeColour(230, 41, 55, 255);
    ui_harbor_palette.warning = MakeColour(255, 255, 176, 255);

    ui_meadow_palette.panel_background = MakeColour(121, 120, 49, 255);
    ui_meadow_palette.container_border = MakeColour(230, 194, 119, 255);
    ui_meadow_palette.container_fill = MakeColour(111, 137, 119, 255);
    ui_meadow_palette.field_row_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_meadow_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.input_fill = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.button_fill = MakeColour(230, 194, 119, 255);
    // Toggle fills: ON reuses the amber button fill; OFF is the same hue dimmed towards the panel.
    ui_meadow_palette.toggle_on_fill = MakeColour(230, 194, 119, 255);
    ui_meadow_palette.toggle_off_fill = MakeColour(156, 132, 81, 255);
    ui_meadow_palette.text = MakeColour(32, 42, 37, 255);
    ui_meadow_palette.label_text = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.error = MakeColour(230, 41, 55, 255);
    ui_meadow_palette.warning = MakeColour(255, 255, 176, 255);

    ui_default_palette = ui_harbor_palette;
    ui_palettes_initialized = true;
}

void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
                                 ColourRgba *out_border, ColourRgba *out_fill)
{
    ColourRgba border = COLOURLESS_RGBA;
    ColourRgba fill = COLOURLESS_RGBA;

    if (!palette)
    {
        palette = &ui_default_palette;
    }

    switch (surface)
    {
        case UI_PALETTE_SURFACE_CONTAINER:
            border = palette->container_border;
            fill = palette->container_fill;
            break;
        case UI_PALETTE_SURFACE_FIELD_ROW:
            border = palette->field_row_border;
            fill = palette->field_row_fill;
            break;
        case UI_PALETTE_SURFACE_INPUT:
            border = palette->input_border;
            fill = palette->input_fill;
            break;
        case UI_PALETTE_SURFACE_BUTTON:
            border = palette->button_border;
            fill = palette->button_fill;
            break;
        case UI_PALETTE_SURFACE_TRANSPARENT:
        default:
            break;
    }

    if (out_border)
    {
        *out_border = border;
    }
    if (out_fill)
    {
        *out_fill = fill;
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
                   a.as.cell.cell_index == b.as.cell.cell_index;
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
        // A cell-only selection records a CELL ref for the currently selected world context,
        // matching the info strip's existing owning-world resolution.
        int world_index = Universe_GetSelectedIndex(&G_Universe);
        SelectionSet_Add((TargetRef){
            .kind = TARGET_KIND_CELL,
            .as.cell = {
                .world_index = (world_index >= 0) ? world_index : UNIVERSE_ROOT_WORLD_INDEX,
                .cell_index = cell_index,
            },
        });
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
                    // Validate a cell via its owning world index (cell index within an existing
                    // world is treated as valid in Stage 1).
                    stale = member->as.cell.world_index != UNIVERSE_ROOT_WORLD_INDEX &&
                            (member->as.cell.world_index < 0 ||
                             member->as.cell.world_index >= Universe_GetWorldCount(&G_Universe));
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
    TargetRef primary = SelectionSet_Primary();
    if (primary.kind != TARGET_KIND_CELL)
    {
        return NULL;
    }

    // Resolve the owning world (handles the UNIVERSE_ROOT_WORLD_INDEX sentinel via Universe_GetWorld).
    World2d *world = Universe_GetWorld(&G_Universe, primary.as.cell.world_index);
    if (!world)
    {
        return NULL;
    }

    // Bounds-check the cell index against the live cell buffer, then index it directly the same
    // way every other cell consumer does (space->cells.items is the dense Cell array).
    Space2d *space = &world->grid_space.space;
    int cell_index = primary.as.cell.cell_index;
    if (!space->cells.items || cell_index < 0 || cell_index >= (int)space->cells.count)
    {
        return NULL;
    }

    return &((Cell *)space->cells.items)[cell_index];
}

// Report the selected cell's index from the SelectionSet primary, or -1 when the primary is not a
// cell. Mirrors UIState_GetSelectedCell's single-source-of-truth derivation.
int UIState_GetSelectedCellIndex(void)
{
    TargetRef primary = SelectionSet_Primary();
    return (primary.kind == TARGET_KIND_CELL) ? primary.as.cell.cell_index : -1;
}

// Return the currently active tool (TOOL_NONE when none is active).
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

void InitUI(void)
{
    InitUIPalettes();

    // Register XML element builders once before any panels are created
    UILoader_RegisterDefaultBuilders();

    // Init Global UI State
    UIState_SetSelection(NULL, NULL, -1);
    G_UIState.focused_element = NULL;
    // The lpanel is retired: it is no longer mounted in the UI lifecycle. Its files (and the
    // space-basis getters) are kept only so diagnostics.c still links against the lpanel-space
    // debug-basis target; the lpanel's viewport slot now hosts the Entity Mgmt panel.
    InitUtilityPanel();
    InitRPanel();
    InitStateManagerSystem();
    InitPopupMenu();
    InitInfoStrip();
    InitDetailStrip();
    InitToolbar();
    G_UIState.active_view = LPANEL_STATE_VIEW;
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

    // Each panel owns and disposes its root, views, selectors, and cached pointers. The lpanel is
    // retired (never initialised), so it is not destroyed here.
    DestroyRPanel();
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
    // The lpanel is retired (never initialised), so it is not drawn here.
    DrawRPanel();
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

// Sync entity-creation editor textboxes when the draw view is active, otherwise clear them.
static void RefreshEntityEditorFields(bool editor_active, EntityCreateParams *params)
{
    TextboxField edit_fields[] = {
        {G_UIState.edit_vertice_count_tbox, INT, (editor_active && params) ? (void *)&params->physics.vertice_count : NULL, 0, NULL},
        {G_UIState.edit_width_tbox, FLOAT, (editor_active && params) ? (void *)&params->physics.width : NULL, 2, NULL},
        {G_UIState.edit_height_tbox, FLOAT, (editor_active && params) ? (void *)&params->physics.height : NULL, 2, NULL},
        {G_UIState.edit_mass_tbox, FLOAT, (editor_active && params) ? (void *)&params->physics.mass : NULL, 2, NULL},
        {G_UIState.edit_restitution_tbox, FLOAT, (editor_active && params) ? (void *)&params->physics.restitution : NULL, 2, NULL},
        {G_UIState.edit_friction_tbox, FLOAT, (editor_active && params) ? (void *)&params->physics.friction : NULL, 2, NULL},
        {G_UIState.edit_pos_c_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->physics.anchor_position : NULL, 0, NULL},
        {G_UIState.edit_vel_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->physics.velocity : NULL, 0, NULL},
        {G_UIState.edit_accel_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->physics.acceleration : NULL, 0, NULL},
        {G_UIState.edit_moment_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->physics.momentum : NULL, 0, NULL},
    };

    RefreshTextboxFields(edit_fields, ARRAY_COUNT(edit_fields));
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

    // COLLECT & UPDATE EDITING ENTITY PROPERTIES
    // The lpanel DRAW edit view has been retired, so the editor fields are never the active view
    // now; refresh with the edit view inactive so the fields stay cleared/idle.
    EntityCreateParams *params = G_UIState.entity_create_params;
    bool edit_view_active = false;
    RefreshEntityEditorFields(edit_view_active, params);
}


