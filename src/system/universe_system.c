/**********************************************************************************************
*
UNIVERSE SYSTEM MODULE
*
**********************************************************************************************/
#include "raylib.h"
#include "world/universe.h"
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
#include "system/command_queue.h"
#include "world/world_internal.h"

static int create_world_auto_select = 0;
static bool camera_diagnostic_printed = false;
static bool click_pending = false;
ColourRgba camera_marker_colour = {86, 139, 127, 230};
static CameraController cam_ctrl = {0};

static void SyncWorldStateFromSelection(void)
{
    UIState_SetSelection(NULL, NULL, -1);
    MarkStateManagerRefreshDirty();
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

    if (input->delete_pressed && !G_UIState.focused_element && G_Universe.selected_world_index >= 0)
    {
        if (EnqueueDeleteWorld(G_Universe.selected_world_index))
        {
            return INPUT_ROUTE_HANDLED;
        }
    }

    UpdateUniverseInput(input, cursor_in_game_viewport);
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    if (game_drag_ctx->has_capture &&
        game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER)
    {
        return INPUT_ROUTE_CAPTURED;
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
                if (World_IsDraggable(world))
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
            bool world_hit = false;
            if (!Universe_ResolveClickHit(mouse_universe_coords, &world_hit))
            {
                G_Universe.selected_world_index = -1;
                SyncWorldStateFromSelection();
            }
            else if (world_hit)
            {
                click_pending = true;
            }
        }
    }
    else if (G_Universe.selected_world_index >= 0 && input->left_pressed && cursor_in_game_viewport)
    {
        if (!Universe_ResolveClickHit(mouse_universe_coords, NULL))
        {
            G_Universe.selected_world_index = -1;
            SyncWorldStateFromSelection();
        }
    }

    Controller_Update(&cam_ctrl);
}

void DrawUniverse(void)
{
    Matrix3x3 camera_to_pixel = ResolveWorldToPixelMatrix(&G_Universe.root_world, &G_Universe.camera);
    CameraViewBox camera_view = GetCameraView(&G_Universe.camera, game_viewport, camera_to_pixel);
    UniverseRenderer_Draw(&G_Universe, camera_view, camera_to_pixel);
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

int *GetCreateWorldAutoSelectPtr(void)
{
    return &create_world_auto_select;
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

bool ConsumeUniverseWorldClick(void)
{
    bool pending = click_pending;
    click_pending = false;
    return pending;
}
