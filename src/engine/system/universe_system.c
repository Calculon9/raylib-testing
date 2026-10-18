/**********************************************************************************************
*
UNIVERSE SYSTEM MODULE
*
**********************************************************************************************/
#include "raylib.h"
#include "world/universe_renderer.h"
#include "world/world.h"
#include "camera/camera.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"
#include "common/common.h"
#include "system/universe_system.h"
#include "system/viewport_system.h"
#include "system/debug_overlay_system.h"
#include "input/drag_interaction.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/ui/state_manager_system.h"
#include "system/ui/popup_menu.h"
#include "system/command_system.h"
#include "world/world_internal.h"
#include "entities/entity_registry.h"

static int create_world_auto_select = 0;
static bool camera_diagnostic_printed = false;
ColourRgba camera_marker_colour = {86, 139, 127, 230};
static CameraController cam_ctrl = {0};

// Refresh panels that mirror the current navigation world. This is a CURSOR concern only: it must
// NOT touch the SelectionSet (the action source of truth). Moving the World Manager cursor
// (PREV/NEXT) therefore no longer wipes or rewrites what is selected for actions, and selecting
// things for actions never moves the cursor.
static void SyncWorldStateFromSelection(void)
{
    MarkStateManagerRefreshDirty();
}

// Order world indices high-to-low in place. Used before enqueuing world deletes so that
// Universe_DeleteWorld's array compaction (which shifts every index above the deleted one down by
// one) can never invalidate a still-pending smaller index. A simple insertion sort over at most
// UNIVERSE_MAX_WORLDS ints; no allocation, and the input set is tiny.
static void SortWorldIndicesDescending(int *indices, int count)
{
    for (int i = 1; i < count; i++)
    {
        int key = indices[i];
        int j = i - 1;
        // Shift elements smaller than key up so larger indices settle towards the front.
        while (j >= 0 && indices[j] < key)
        {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }
}

static bool Universe_HasRootEntityAt(Vector2d universe_point)
{
    return ResolveClosestEntityAt(&G_Universe.root_world, universe_point,
                                  NULL, NULL, NULL, 0, NULL) != NULL;
}

// Single entry point for "what did this universe click hit": a nested world, a root-owned
// entity, or nothing. Callers no longer need to combine the two checks themselves.
static bool Universe_ResolveClickHit(Vector2d click_universe_coords, bool *world_hit_out)
{
    bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);
    bool root_entity_hit = Universe_HasRootEntityAt(click_universe_coords);
    if (world_hit_out)
    {
        *world_hit_out = world_hit;
    }
    return world_hit || root_entity_hit;
}

static void ApplyWorldDragTransform(World2d *world, Vector2d new_universe_origin)
{
    if (!world)
    {
        return;
    }

    world->grid_space.space.frame.origin_in_parent = new_universe_origin;
    world->uni_coords_center = new_universe_origin;
    BindWorldTunnel(world, &G_Universe.camera);

    World_RefreshBoundsFromFrame(world);
}

static bool World_IsDraggable(const World2d *world)
{
    return world && (world->flags & WORLD_FLAG_DRAGGABLE) != 0;
}

void SyncUniverseCameraToViewport(void)
{
    Camera_SetDestinationFrame(&G_Universe.camera, &game_viewport.frame);
    Camera_SetSourceFrame(&G_Universe.camera, &G_Universe.camera.frame);
}

bool SetUniverseCameraBasis(Basis2d basis)
{
    Basis2d normalised_basis;
    if (!Basis2d_NormaliseAndValidate(basis, &normalised_basis))
    {
        return false;
    }

    float u_mag = VectorMagnitude_2d(normalised_basis.u);
    G_Universe.camera.rotation = VectorRadians_2d(normalised_basis.u);
    G_Universe.camera.zoom = u_mag;

    if (cam_ctrl.camera == &G_Universe.camera)
    {
        cam_ctrl.target_zoom = G_Universe.camera.zoom;
        cam_ctrl.target_source_focus_coords = G_Universe.camera.source_focus_coords;
    }

    UpdateCameraFull(&G_Universe.camera);
    return true;
}

void InitUniverseSystem(void)
{
    extern float gravity;

    EntityRegistry_Init();
    Vector2d universe_resolution = UniverseRenderer_GetResolution();
    G_Universe.resolution = universe_resolution;
    Universe_Init(&G_Universe, ZERO_VECTOR_2D, (Vector2d){7, 5}, gravity);

    Vector2d default_lens_size = {
        game_viewport.frame.local_max.x - game_viewport.frame.local_min.x,
        game_viewport.frame.local_max.y - game_viewport.frame.local_min.y};
    G_Universe.camera = CreateCamera2d(&G_Universe.camera.frame, &game_viewport.frame);
    G_Universe.camera.frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, default_lens_size);
    G_Universe.camera.source_focus_coords = ZERO_VECTOR_2D;
    G_Universe.camera.rotation = 0.0f;
    G_Universe.camera.zoom = 1.0f;

    SyncUniverseCameraToViewport();
    cam_ctrl = CreateCameraController(&G_Universe.camera);

    int initial_world_columns = (int)fmaxf(1.0f, ceilf(G_Universe.next_resolution.x));
    int initial_world_rows = (int)fmaxf(1.0f, ceilf(G_Universe.next_resolution.y));
    *GetNextWorldObjectCountPtr() = 1; // (initial_world_columns * initial_world_rows) / 3 + 1;
    CreateNewWorld(false);
}

InputRouteResult UpdateUniverseSystem(const InputFrame *input, InputRouteResult prior_result)
{
    if (!input || prior_result != INPUT_ROUTE_IGNORED)
    {
        return prior_result;
    }

    Matrix3x3 root_world_to_pixel = ResolveWorldToPixelMatrix(&G_Universe.root_world, &G_Universe.camera);
    bool cursor_in_game_viewport = ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position);

    if (!camera_diagnostic_printed)
    {
        Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();
        Vector2d game_viewport_pixel_centre = TransformCoordinates(root_world_to_pixel, game_viewport_local_centre);
        LOG_INFO("[ROOT CAMERA DIAG] dest_origin=(%.2f, %.2f) src_focus=(%.2f, %.2f) viewport_px_center=(%.2f, %.2f) viewport_local_center=(%.2f, %.2f)\n",
                 G_Universe.camera.tunnel.destination_frame->origin_in_parent.x, G_Universe.camera.tunnel.destination_frame->origin_in_parent.y,
                 G_Universe.camera.source_focus_coords.x, G_Universe.camera.source_focus_coords.y,
                 game_viewport_pixel_centre.x, game_viewport_pixel_centre.y,
                 game_viewport_local_centre.x, game_viewport_local_centre.y);
        camera_diagnostic_printed = true;
    }

    // DELETE acts on the central SelectionSet (the single source of truth for actions), NOT on the
    // World Manager navigation cursor (selected_world_index). A current-but-unselected world is
    // kept; a selected-but-not-current world is deleted.
    if (input->delete_pressed && !G_UIState.focused_element && SelectionSet_Count() > 0)
    {
        // World indices must be enqueued high-to-low: Universe_DeleteWorld compacts the array,
        // shifting every index above the deleted one down by one, so deleting the largest index
        // first leaves the still-pending (smaller) indices valid. Entities carry stable ids and
        // are order-independent, so enqueue them directly while scanning.
        int world_indices[UNIVERSE_MAX_WORLDS]; // bounded by the universe's world cap (stack, no alloc)
        int world_count = 0;

        TargetRef *member = NULL;
        LArray_ForEach(&G_UIState.selection_set, TargetRef *, member)
        {
            switch (member->kind)
            {
                case TARGET_KIND_ENTITY:
                    EnqueueDeleteEntity(member->as.entity); // reuse the existing entity-delete path
                    break;
                case TARGET_KIND_WORLD:
                    if (world_count < UNIVERSE_MAX_WORLDS)
                    {
                        world_indices[world_count++] = member->as.world_index; // defer, sort later
                    }
                    break;
                case TARGET_KIND_CELL:
                default:
                    // CELL (and any NONE) members have no delete action in this stage; skip.
                    break;
            }
        }

        // Enqueue world deletes in descending index order so compaction never invalidates a
        // pending index (see ordering-hazard note above).
        SortWorldIndicesDescending(world_indices, world_count);
        for (int i = 0; i < world_count; i++)
        {
            EnqueueDeleteWorld(world_indices[i]); // reuse the existing world-delete path
        }

        // Clear the set now that every member has been enqueued: enqueue-all-then-clear keeps the
        // ForEach loop reading a stable set, and dropping all refs avoids any stale/aliased world
        // TargetRef surviving the index compaction. Keep the back-compat primary fields in step.
        SelectionSet_Clear();
        UIState_SyncPrimaryFieldsFromSet();

        return INPUT_ROUTE_HANDLED;
    }

    UpdateUniverseInput(input, cursor_in_game_viewport);
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    if (game_drag_ctx->has_capture &&
        game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER)
    {
        return INPUT_ROUTE_CAPTURED;
    }

    // Let the world route capture root-owned entities even when a nested world remains selected.
    if (input->left_pressed && cursor_in_game_viewport)
    {
        Vector2d mouse_universe_coords = TransformCoordinates(root_world_to_pixel,
                                                               input->pointer_position);
        if (Universe_HasRootEntityAt(mouse_universe_coords))
        {
            return INPUT_ROUTE_IGNORED;
        }
    }

    // Hold the frame for a SELECTED world while the button is down, even before the drag crosses
    // its pixel threshold. Those sub-threshold left_down frames would otherwise return IGNORED and
    // let the world route's cell pick run UIState_SetSelection, which REPLACES the WORLD ref in the
    // SelectionSet with a CELL ref. The world-drag gate (World_IsDraggable && SelectionSet_Contains
    // for the WORLD ref) would then fail once the threshold is finally crossed, and the drag would
    // fall through to a marquee. Claiming the frame here keeps the WORLD ref intact so a selected
    // world can be dragged from anywhere inside its bounds, regardless of a coincident cell hit.
    // Gated on the world being BOTH draggable and already selected, so an UNSELECTED world still
    // falls through to the marquee path unchanged.
    if (input->left_down && cursor_in_game_viewport)
    {
        Vector2d mouse_universe_coords = TransformCoordinates(root_world_to_pixel,
                                                               input->pointer_position);
        int world_index = Universe_FindWorldAt(&G_Universe, mouse_universe_coords);
        if (world_index >= 0)
        {
            TargetRef world_ref = { .kind = TARGET_KIND_WORLD, .as.world_index = world_index };
            if (World_IsDraggable(&G_Universe.worlds[world_index]) &&
                SelectionSet_Contains(world_ref))
            {
                return INPUT_ROUTE_HANDLED;
            }
        }
    }

    if (input->left_pressed || input->left_released)
    {
        return INPUT_ROUTE_HANDLED;
    }

    return INPUT_ROUTE_IGNORED;
}

void UpdateUniverseInput(const InputFrame *input, bool cursor_in_game_viewport)
{
    if (!input)
    {
        return;
    }

    Matrix3x3 pixel_to_universe = ResolvePixelToWorldMatrix(&G_Universe.root_world, &G_Universe.camera);
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    Vector2d mouse_pixel_coords = input->pointer_position;
    Vector2d mouse_universe_coords = TransformCoordinates(pixel_to_universe, mouse_pixel_coords);

    if (cursor_in_game_viewport)
    {
        float wheel_move = input->wheel_delta;
        Vector2d pan_delta = ZERO_VECTOR_2D;
        float current_speed = cam_ctrl.base_pan_speed / cam_ctrl.target_zoom;

        if (IsKeyDown(KEY_W))
            pan_delta.y -= current_speed;
        if (IsKeyDown(KEY_S))
            pan_delta.y += current_speed;
        if (IsKeyDown(KEY_A))
            pan_delta.x -= current_speed;
        if (IsKeyDown(KEY_D))
            pan_delta.x += current_speed;

        if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
        {
            Controller_Pan(&cam_ctrl, pan_delta, frame_counter.delta_time);
        }

        if (IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (wheel_move > 0.0f)
                cam_ctrl.target_zoom *= cam_ctrl.zoom_speed;
            else if (wheel_move < 0.0f)
                cam_ctrl.target_zoom /= cam_ctrl.zoom_speed;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT))
        {
            if (wheel_move > 0.0f)
                Controller_Rotate(&cam_ctrl, -0.05f);
            else if (wheel_move < 0.0f)
                Controller_Rotate(&cam_ctrl, 0.05f);
        }
    }

    if (cursor_in_game_viewport && input->left_down && DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
    {
        // Check if drag threshold has been exceeded and apply updates if so
        if (!game_drag_ctx->has_capture)
        {
            int world_index = Universe_FindWorldAt(&G_Universe, mouse_universe_coords);
            if (world_index >= 0)
            {
                World2d *world = &G_Universe.worlds[world_index];
                // Gate world-drag on selection: a world is only draggable once it is in the
                // SelectionSet, extending the engine's "selection arms a thing for dragging" rule
                // to worlds. A drag inside an unselected world is NOT captured here, so it falls
                // through to the world stage's marquee capture path.
                TargetRef world_ref = { .kind = TARGET_KIND_WORLD, .as.world_index = world_index };
                if (World_IsDraggable(world) && SelectionSet_Contains(world_ref))
                {
                    DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_CONTAINER, world,
                                                 world->grid_space.space.frame.origin_in_parent);
                }
            }
        }

        if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER)
        {
            World2d *world = (World2d *)game_drag_ctx->target;
            if (!World_IsDraggable(world))
            {
                DragInteraction_ClearCapture(game_drag_ctx);
                return;
            }

            Vector2d initial = TransformCoordinates(pixel_to_universe, game_drag_ctx->pointer_state.initial_pos);
            Vector2d current = TransformCoordinates(pixel_to_universe, game_drag_ctx->pointer_state.current_pos);
        Vector2d delta = VectorDiff_2d(current, initial);
            ApplyWorldDragTransform(world, VectorSum_2d(game_drag_ctx->target_anchor, delta));
        }
    }
    else if (game_drag_ctx->pointer_state.left_button_hold_ticks > 0)
    {
        bool was_click = DragInteraction_IsClick(game_drag_ctx, INPUT_CLICK_MAX_HOLD_TICKS,
                                                 INPUT_DRAG_THRESHOLD_PIXELS);
        if (was_click && cursor_in_game_viewport)
        {
            if (input->ctrl_down || input->shift_down)
            {
                // Modified click: toggle the hit world into the set WITHOUT the selecting side
                // effect (so it coexists with selected entities and does not disturb
                // selected_world_index). Empty space leaves the set intact.
                int hit_index = Universe_FindWorldAt(&G_Universe, mouse_universe_coords);
                if (hit_index >= 0)
                {
                    SelectionSet_Toggle((TargetRef){ .kind = TARGET_KIND_WORLD, .as.world_index = hit_index });
                    UIState_SyncPrimaryFieldsFromSet();
                }
            }
            else
            {
                // Unmodified click: existing plain select / empty-click deselect.
                bool world_hit = false;
                if (!Universe_ResolveClickHit(mouse_universe_coords, &world_hit))
                {
                    G_Universe.selected_world_index = -1; // cursor: nothing focused
                    SyncWorldStateFromSelection();        // cursor refresh only
                    SelectionSet_Clear();                 // empty-click deselects everything (action set)
                    UIState_SyncPrimaryFieldsFromSet();
                }
                else if (world_hit)
                {
                    // Plain click on a world: REPLACE the action set with just this world,
                    // mirroring the entity plain-click (clear-then-add-one). Universe_ResolveClickHit
                    // has already moved the navigation cursor (selected_world_index) via
                    // Universe_SelectWorld; that cursor is deliberately kept separate from the action
                    // set. Arming the world as the sole SelectionSet member makes the world-drag gate
                    // (World_IsDraggable && SelectionSet_Contains) pass, so the world can then be dragged.
                    int hit_index = Universe_FindWorldAt(&G_Universe, mouse_universe_coords);
                    if (hit_index >= 0)
                    {
                        SelectionSet_Clear(); // REPLACE: drop any prior selection
                        SelectionSet_Add((TargetRef){ .kind = TARGET_KIND_WORLD, .as.world_index = hit_index });
                        UIState_SyncPrimaryFieldsFromSet(); // keep back-compat primary fields in step
                    }
                }
            }
        }
    }
    else if (G_Universe.selected_world_index >= 0 && input->left_pressed && cursor_in_game_viewport)
    {
        if (!Universe_ResolveClickHit(mouse_universe_coords, NULL))
        {
            G_Universe.selected_world_index = -1; // cursor: nothing focused
            SyncWorldStateFromSelection();        // cursor refresh only
            SelectionSet_Clear();                 // empty-click deselects everything (action set)
            UIState_SyncPrimaryFieldsFromSet();
        }
    }

    Controller_Update(&cam_ctrl);
}

void DrawUniverse(void)
{
    Matrix3x3 camera_to_pixel = ResolveWorldToPixelMatrix(&G_Universe.root_world, &G_Universe.camera);
    CameraViewBox camera_view = GetCameraView(&G_Universe.camera, game_viewport, camera_to_pixel);
    UniverseRenderer_Draw(&G_Universe, camera_view, camera_to_pixel);

    // Overlay the selection highlight pass on top of the drawn worlds/entities, in world space.
    WorldRenderer_DrawSelectionHighlights(&G_Universe);

    // Draw the live marquee box (pixel space) above the highlights while a box drag is active.
    WorldRenderer_DrawMarquee();
}

int CreateNewWorld(bool auto_select)
{
    int index = Universe_CreateWorld(&G_Universe, COLOUR_GAME_PARCHMENT_RGBA, COLOUR_GAME_OLIVE_RGBA,
                                     camera_marker_colour, G_Universe.next_spawn, auto_select);
    if (index >= 0 && auto_select)
    {
        SyncWorldStateFromSelection();
    }
    return index;
}

int CreateNewWorld_Preset(bool auto_select, CoordinateSpacePreset preset)
{
    Vector2d previous_resolution = G_Universe.next_resolution;
    Vector2d previous_basis_u = G_Universe.next_basis_u;
    Vector2d previous_basis_v = G_Universe.next_basis_v;

    G_Universe.next_resolution = preset.resolution;
    G_Universe.next_basis_u = preset.basis.u;
    G_Universe.next_basis_v = preset.basis.v;

    int index = CreateNewWorld(auto_select);

    G_Universe.next_resolution = previous_resolution;
    G_Universe.next_basis_u = previous_basis_u;
    G_Universe.next_basis_v = previous_basis_v;

    if (index >= 0 && auto_select)
    {
        SyncWorldStateFromSelection();
    }
    return index;
}

bool SelectWorldByIndex(int index)
{
    bool ok = Universe_SelectWorld(&G_Universe, index);
    if (ok)
    {
        SyncWorldStateFromSelection();
    }
    return ok;
}

bool IsCreateWorldAutoSelectEnabled(void)
{
    return create_world_auto_select != 0;
}

int GetWorldCount(void) { return Universe_GetWorldCount(&G_Universe); }
int GetSelectedWorldIndex(void) { return Universe_GetSelectedIndex(&G_Universe); }
World2d *GetSelectedWorld(void) { return Universe_GetSelectedWorld(&G_Universe); }
World2d *GetWorldByIndex(int index) { return Universe_GetWorld(&G_Universe, index); }

Vector2d *GetNextWorldSpawnOriginPtr(void) { return &G_Universe.next_spawn; }
Vector2d *GetNextWorldResolutionPtr(void) { return &G_Universe.next_resolution; }
Vector2d *GetNextWorldBasisUPtr(void) { return &G_Universe.next_basis_u; }
Vector2d *GetNextWorldBasisVPtr(void) { return &G_Universe.next_basis_v; }
float *GetNextWorldGravityPtr(void) { return &G_Universe.next_gravity; }
int *GetNextWorldObjectCountPtr(void) { return &G_Universe.next_object_count; }

