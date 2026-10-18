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
#include "world/target_resolver.h"
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

// Press-resolved interaction state for the game viewport. Resolved ONCE on left_pressed and read
// on the subsequent left_down/left_released frames so no stage re-picks the pointer per frame.
typedef struct GamePress
{
    TargetRef select_ref;   // Full resolver result at press (entity/cell/world/none) -> what a CLICK commits.
    bool      valid;        // True between left_pressed and left_released for a viewport press.
    bool      modified;     // Ctrl/Shift held at press time -> toggle instead of replace on commit.
    bool      armed_move;   // True if the press escalated to a movable, already-selected target and armed a capture.
    bool      ever_dragged; // True once the capture crosses the 5px threshold (drives the release decision).
} GamePress;

// A single module-local record (no allocation), reset on left_pressed and consumed on
// left_released. The escalated drag target itself is NOT stored: the DragInteraction capture
// already holds the target + anchor, so only the fact that a move was armed matters afterwards.
static GamePress g_game_press = {0};

// Report whether a target ref denotes a thing that can be dragged. ENTITY is always movable; a
// WORLD is movable only when it is a valid, draggable NESTED world (the universe root world is a
// navigation sentinel and is never draggable); CELL and NONE are never movable. The WORLD branch
// bounds-checks the index before dereferencing G_Universe.worlds so a stale/compacted index (e.g.
// after a mid-press delete) can never index out of range.
static bool RefIsMovable(TargetRef ref)
{
    switch (ref.kind)
    {
        case TARGET_KIND_ENTITY:
            return true;
        case TARGET_KIND_WORLD:
            // Reject the root sentinel and any out-of-range index BEFORE indexing the world array.
            if (ref.as.world_index < 0 || ref.as.world_index >= G_Universe.world_count)
            {
                return false; // covers UNIVERSE_ROOT_WORLD_INDEX (-2) and stale indices
            }
            return World_IsDraggable(&G_Universe.worlds[ref.as.world_index]);
        case TARGET_KIND_CELL:
        case TARGET_KIND_NONE:
        default:
            return false;
    }
}

// Compute the drag target for a press: "what movable, already-selected target contains this
// point?". This is a separate lookup from the selection resolve (select_ref) and does NOT call
// Target_ResolveAtPoint again. Precedence (first match wins):
//   1. A selected ENTITY the resolver hit directly (entities are unconditionally movable, so
//      membership is tested directly rather than via RefIsMovable).
//   2. Else the owning nested world under the point (Universe_FindWorldAt), if it is movable
//      (valid + draggable) AND already selected -> that WORLD. This is what lets a press on a CELL
//      inside a selected draggable world escalate to dragging the WORLD.
//   3. Else NONE (a subsequent drag becomes a marquee; a release commits the select_ref).
static TargetRef EscalateDragTarget(TargetRef select_ref, Vector2d pointer_universe)
{
    // Step 1: a selected entity the resolver hit on its polygon.
    if (select_ref.kind == TARGET_KIND_ENTITY && SelectionSet_Contains(select_ref))
    {
        return select_ref;
    }

    // Step 2: the owning nested world, if movable and selected.
    int owning = Universe_FindWorldAt(&G_Universe, pointer_universe);
    if (owning >= 0)
    {
        TargetRef world_ref = { .kind = TARGET_KIND_WORLD, .as.world_index = owning };
        if (RefIsMovable(world_ref) && SelectionSet_Contains(world_ref))
        {
            return world_ref;
        }
    }

    // Step 3: nothing movable and selected under the press.
    return (TargetRef){ .kind = TARGET_KIND_NONE };
}

// Commit a click selection from the pending press's select_ref. Plain (no modifier) REPLACES the
// set (clear then add); a NONE ref additionally clears the navigation cursor and refreshes the
// cursor panels (the empty-space deselect, now on release); a WORLD ref also moves the navigation
// cursor exactly as the old plain-world click did (via Universe_ResolveClickHit ->
// Universe_SelectWorld). A modifier TOGGLES the ref into/out of the set (+ WORLD cursor sync). The
// back-compat primary single-fields are re-synced from the set afterwards.
static void CommitClickSelection(Vector2d pointer_universe)
{
    TargetRef select_ref = g_game_press.select_ref;

    if (!g_game_press.modified)
    {
        // Plain click: replace the whole action set with the single resolved ref.
        if (select_ref.kind == TARGET_KIND_NONE)
        {
            // Empty space: deselect everything and drop the navigation cursor.
            G_Universe.selected_world_index = -1; // cursor: nothing focused
            SyncWorldStateFromSelection();        // cursor refresh only
            SelectionSet_Clear();                 // action set: empty-click deselects everything
        }
        else
        {
            SelectionSet_Clear(); // REPLACE: drop any prior selection
            if (select_ref.kind == TARGET_KIND_WORLD)
            {
                // Move the navigation cursor to the clicked world (cursor side-effect only), then
                // add the WORLD ref to the action set. Universe_ResolveClickHit calls
                // Universe_SelectWorld, matching the old plain-world click cursor behaviour.
                Universe_ResolveClickHit(pointer_universe, NULL);
            }
            SelectionSet_Add(select_ref);
        }
    }
    else
    {
        // Modified click: toggle the ref into/out of the set (ignores NONE). For a WORLD also move
        // the navigation cursor to it when it was just added, mirroring the plain-world cursor sync.
        if (select_ref.kind == TARGET_KIND_WORLD && !SelectionSet_Contains(select_ref))
        {
            Universe_ResolveClickHit(pointer_universe, NULL);
        }
        SelectionSet_Toggle(select_ref);
    }

    UIState_SyncPrimaryFieldsFromSet(); // keep the back-compat primary fields in step
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

    // A live world-container drag owns the frame on both down AND release frames: the capture is
    // torn down by DragInteraction_EndContextFrame at frame end, so returning CAPTURED here keeps
    // any other stage from claiming it. Entity captures and the marquee deliberately fall through
    // to IGNORED so the (unconditionally-run) world stage drives the entity move / marquee commit.
    if (game_drag_ctx->has_capture &&
        game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER)
    {
        return INPUT_ROUTE_CAPTURED;
    }

    // The generic classifier in UpdateUniverseInput has already committed any click selection and
    // armed/driven captures. A press or release frame is consumed here (HANDLED); on a plain down
    // frame we return IGNORED so the world stage can drive an armed entity capture.
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

    // ------------------------------------------------------------------------------------------
    // Generic game-viewport interaction: resolve -> escalate -> classify (click=select /
    // drag-selected=move / drag-unselected=marquee). Selection is committed on RELEASE-as-click,
    // never on the bare down frame, so no sub-threshold frame can clobber an existing selection.
    // ------------------------------------------------------------------------------------------

    if (input->left_pressed && cursor_in_game_viewport)
    {
        // PRESS: resolve the selection ref once and remember the press. Do NOT touch the
        // SelectionSet or the navigation cursor on this frame.
        g_game_press = (GamePress){
            .select_ref   = Target_ResolveAtPoint(&G_Universe, mouse_universe_coords),
            .valid        = true,
            .modified     = input->ctrl_down || input->shift_down,
            .armed_move   = false,
            .ever_dragged = false,
        };

        // Only a plain (unmodified) press arms a move; a modified press is always a toggle-click,
        // so it never begins a drag (dragging to toggle is not a gesture the model supports).
        if (!g_game_press.modified)
        {
            TargetRef drag_target = EscalateDragTarget(g_game_press.select_ref, mouse_universe_coords);
            if (drag_target.kind == TARGET_KIND_ENTITY)
            {
                // Entity leaf arm lives in the world stage (it owns the snapshot statics).
                g_game_press.armed_move = WorldSystem_ArmEntityDragById(drag_target.as.entity, false);
            }
            else if (drag_target.kind == TARGET_KIND_WORLD)
            {
                // World leaf arm: begin the container capture with the world's current origin as
                // anchor (the same anchor the old lazy threshold-frame path used).
                World2d *world = &G_Universe.worlds[drag_target.as.world_index];
                DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_CONTAINER, world,
                                             world->grid_space.space.frame.origin_in_parent);
                g_game_press.armed_move = game_drag_ctx->has_capture;
            }
        }
    }
    else if (input->left_down && g_game_press.valid && cursor_in_game_viewport)
    {
        // DOWN: drive whatever was armed, or begin a marquee once an unselected-target press drags.
        if (game_drag_ctx->has_capture)
        {
            // Record that the gesture became a real drag (drives the release classification).
            if (DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
            {
                g_game_press.ever_dragged = true;
            }

            // World container moves are applied here; entity captures are driven by the world
            // stage (it owns HandleCapturedEntityDrag), and the marquee just tracks the pointer.
            if (game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER &&
                DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
            {
                World2d *world = (World2d *)game_drag_ctx->target;
                if (!World_IsDraggable(world))
                {
                    DragInteraction_ClearCapture(game_drag_ctx);
                }
                else
                {
                    Vector2d initial = TransformCoordinates(pixel_to_universe, game_drag_ctx->pointer_state.initial_pos);
                    Vector2d current = TransformCoordinates(pixel_to_universe, game_drag_ctx->pointer_state.current_pos);
                    Vector2d delta = VectorDiff_2d(current, initial);
                    ApplyWorldDragTransform(world, VectorSum_2d(game_drag_ctx->target_anchor, delta));
                }
            }
        }
        else if (DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
        {
            // Nothing armed + the press has dragged => the press began on an unselected target
            // (empty space, unselected world/cell/entity). Begin a marquee (anchored at the press
            // position). This is the generic home of the former world-stage marquee begin.
            DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_MARQUEE, NULL,
                                         game_drag_ctx->pointer_state.initial_pos);
            g_game_press.ever_dragged = true;
        }
    }
    else if (input->left_released && g_game_press.valid)
    {
        // RELEASE: classify the gesture. A capture that never crossed the drag threshold is a
        // CLICK, not a zero-length move, so it runs the selection-commit. Marquee and entity
        // captures are committed/finished by the world stage's release handlers (which run
        // unconditionally); the WORLD_CONTAINER capture is torn down at frame end. The only
        // release work this stage performs directly is the click/no-op selection commit, done
        // when there is no active capture. A modified click always lands here (never armed).
        if (!game_drag_ctx->has_capture)
        {
            bool is_click = DragInteraction_IsClick(game_drag_ctx, INPUT_CLICK_MAX_HOLD_TICKS,
                                                    INPUT_DRAG_THRESHOLD_PIXELS);
            if (is_click && cursor_in_game_viewport)
            {
                CommitClickSelection(mouse_universe_coords); // R5: click -> commit selection
            }
            // else R6: press-hold past the click window with no drag -> no-op, set unchanged.
        }
        else if (!g_game_press.ever_dragged &&
                 (game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY ||
                  game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER) &&
                 DragInteraction_IsClick(game_drag_ctx, INPUT_CLICK_MAX_HOLD_TICKS,
                                         INPUT_DRAG_THRESHOLD_PIXELS))
        {
            // R4: armed a move but never crossed the threshold => treat as a CLICK. The move
            // handlers never ran (travel < 5px), so there is nothing to undo; commit the selection
            // on the resolved ref. The capture itself is cleared at frame end (world container) or
            // by the world stage's entity release handler.
            if (cursor_in_game_viewport)
            {
                CommitClickSelection(mouse_universe_coords);
            }
        }
        // R1/R3 (a real marquee/move release): no selection change here; the world-stage release
        // handlers commit the marquee / restore entity physics, and the capture is torn down at
        // frame end.

        g_game_press.valid = false;
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

