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
#include "system/drag_interaction.h"
#include "system/systems.h"
#include "system/ui/popup_menu.h"

static int create_world_auto_select = 0;
static bool universe_camera_diagnostic_printed = false;
static bool world_click_pending = false;
ColourRgba camera_marker_colour = COLOUR_GAME_TEAL_RGBA;
Frame2d universe_frame = {0};
FrameTunnel universe_tunnel = {0};
static CameraController cam_ctrl = {0};

static void SyncWorldStateFromSelection(void)
{
    G_UIState.selected_object = NULL;
    G_UIState.selected_cell = NULL;
    G_UIState.selected_cell_index = -1;
}

static Matrix3x3 BuildCameraToScreenMatrix(void)
{
    return MatrixMultiply_3x3_3x3(
        game_viewport.tunnel.source_to_dest_mtx,
        G_Universe.camera.tunnel.source_to_dest_mtx);
}

static void ApplyWorldDragTransform(World2d *world, Vector2d new_universe_origin)
{
    if (!world)
    {
        return;
    }

    world->tunnel.source_frame = &world->grid_space.space.frame;
    world->tunnel.destination_frame = &G_Universe.camera.frame;
    world->grid_space.space.frame.origin_in_parent = new_universe_origin;
    world->uni_coords_center = new_universe_origin;
    world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*world->tunnel.source_frame);
    world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(world->tunnel.source_to_dest_mtx);

    int world_index = (int)(world - G_Universe.worlds);
    if (world_index >= 0 && world_index < G_Universe.world_count)
    {
        Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&world->grid_space.space.frame);
        Universe_SetWorldBounds(&G_Universe, world_index, world_bounds.col1, world_bounds.col2);
    }
}

void SyncUniverseCameraToViewport(void)
{
    Camera_SetDestinationFrame(&G_Universe.camera, &game_viewport.frame);
    Camera_SetSourceFrame(&G_Universe.camera, &G_Universe.camera.frame);
}

bool SetUniverseCameraBasis(Basis2d basis)
{
    float u_mag = VectorMagnitude_2d(basis.u);
    float v_mag = VectorMagnitude_2d(basis.v);
    if (u_mag < 0.0001f || v_mag < 0.0001f)
    {
        return false;
    }

    G_Universe.camera.rotation = VectorRadians_2d(basis.u);
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

    Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();
    universe_frame = CreateFrame2d(IDENTITY_BASIS_2D, game_viewport_local_centre, universe_resolution);

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
    *GetNextWorldObjectCountPtr() = (initial_world_columns * initial_world_rows) / 3 + 1;
    CreateNewWorld(false);
}

void UpdateUniverseSystem(int mouse_x, int mouse_y)
{
    Matrix3x3 root_world_to_pixel = BuildCameraToScreenMatrix();
    bool cursor_in_game_viewport = mouse_x >= game_viewport.pixel_origin.x &&
                                   mouse_x <= (game_viewport.pixel_origin.x + (game_viewport.pixel_u.x * game_viewport.resolution.x)) &&
                                   mouse_y >= game_viewport.pixel_origin.y &&
                                   mouse_y <= (game_viewport.pixel_origin.y + (game_viewport.pixel_v.y * game_viewport.resolution.y));

    if (!universe_camera_diagnostic_printed)
    {
        Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();
        Vector2d game_viewport_pixel_centre = TransformCoordinates(root_world_to_pixel, game_viewport_local_centre);
        LOG_INFO("[ROOT CAMERA DIAG] dest_origin=(%.2f, %.2f) src_focus=(%.2f, %.2f) viewport_px_center=(%.2f, %.2f) viewport_local_center=(%.2f, %.2f)\n",
                 G_Universe.camera.tunnel.destination_frame->origin_in_parent.x, G_Universe.camera.tunnel.destination_frame->origin_in_parent.y,
                 G_Universe.camera.source_focus_coords.x, G_Universe.camera.source_focus_coords.y,
                 game_viewport_pixel_centre.x, game_viewport_pixel_centre.y,
                 game_viewport_local_centre.x, game_viewport_local_centre.y);
        universe_camera_diagnostic_printed = true;
    }

    UpdateUniverseInput(mouse_x, mouse_y, cursor_in_game_viewport);
}

void UpdateUniverseInput(int mouse_x, int mouse_y, bool cursor_in_game_viewport)
{
    Matrix3x3 pixel_to_world = MatrixInvert_3x3(BuildCameraToScreenMatrix());
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    Vector2d mouse_pixel_coords = {(float)mouse_x, (float)mouse_y};

    if (cursor_in_game_viewport)
    {
        float wheel_move = GetMouseWheelMove();
        Vector2d pan_delta = ZERO_VECTOR_2D;
        float current_speed = cam_ctrl.base_pan_speed / cam_ctrl.target_zoom;

        if (IsKeyDown(KEY_UP)) pan_delta.y -= current_speed;
        if (IsKeyDown(KEY_DOWN)) pan_delta.y += current_speed;
        if (IsKeyDown(KEY_LEFT)) pan_delta.x -= current_speed;
        if (IsKeyDown(KEY_RIGHT)) pan_delta.x += current_speed;

        if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
        {
            Controller_Pan(&cam_ctrl, pan_delta, frame_counter.delta_time);
        }

        if (IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (wheel_move > 0.0f) cam_ctrl.target_zoom *= cam_ctrl.zoom_speed;
            else if (wheel_move < 0.0f) cam_ctrl.target_zoom /= cam_ctrl.zoom_speed;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT))
        {
            if (wheel_move > 0.0f) Controller_Rotate(&cam_ctrl, -0.05f);
            else if (wheel_move < 0.0f) Controller_Rotate(&cam_ctrl, 0.05f);
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            if (IsPopupMenuVisible())
            {
                HidePopupMenu();
            }
            else
            {
                Vector2d popup_position = TransformCoordinates(
                    game_viewport.tunnel.dest_to_source_mtx, mouse_pixel_coords);
                ShowPopupMenu(popup_position);
            }
        }
    }

    if (G_Universe.selected_world_index < 0 && cursor_in_game_viewport && IsMouseButtonDown((int)MOUSE_BUTTON_LEFT))
    {
        DragInteraction_UpdateButtonDown(game_drag_ctx, mouse_pixel_coords);

        if (game_drag_ctx->pointer_state.left_button_hold_ticks == 1)
        {
            Vector2d click_universe_coords = TransformCoordinates(pixel_to_world, mouse_pixel_coords);
            int world_index = Universe_FindWorldAt(&G_Universe, click_universe_coords);
            if (world_index >= 0 && world_index != G_Universe.selected_world_index)
            {
                World2d *world = &G_Universe.worlds[world_index];
                DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_CONTAINER, world,
                                             world->grid_space.space.frame.origin_in_parent);
            }
            else
            {
                DragInteraction_ClearCapture(game_drag_ctx);
            }
        }

        if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_CONTAINER &&
            DragInteraction_IsDragActive(game_drag_ctx, 5.0f))
        {
            World2d *world = (World2d *)game_drag_ctx->target;
            if (world)
            {
                Vector2d initial = TransformCoordinates(pixel_to_world, game_drag_ctx->pointer_state.initial_pos);
                Vector2d current = TransformCoordinates(pixel_to_world, game_drag_ctx->pointer_state.current_pos);
                Vector2d delta = VectorSum_2d(current, (Vector2d){-initial.x, -initial.y});
                ApplyWorldDragTransform(world, VectorSum_2d(game_drag_ctx->target_anchor, delta));
            }
        }
    }
    else if (G_Universe.selected_world_index < 0 && game_drag_ctx->pointer_state.left_button_hold_ticks > 0)
    {
        bool was_click = DragInteraction_IsClick(game_drag_ctx, 20, 5.0f);
        if (was_click && cursor_in_game_viewport)
        {
            Vector2d click_universe_coords = TransformCoordinates(pixel_to_world, mouse_pixel_coords);
            bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);
            if (!world_hit) G_Universe.selected_world_index = -1;
            else world_click_pending = true;
            SyncWorldStateFromSelection();
        }

        DragInteraction_UpdateButtonUp(game_drag_ctx);
    }
    else if (G_Universe.selected_world_index >= 0 && IsMouseButtonPressed((int)MOUSE_BUTTON_LEFT) && cursor_in_game_viewport)
    {
        Vector2d click_universe_coords = TransformCoordinates(pixel_to_world, mouse_pixel_coords);
        if (!Universe_ResolveClick(&G_Universe, click_universe_coords, NULL))
        {
            G_Universe.selected_world_index = -1;
        }
        SyncWorldStateFromSelection();
    }

    Controller_Update(&cam_ctrl);
}

void DrawUniverse(void)
{
    Matrix3x3 camera_to_pixel = BuildCameraToScreenMatrix();
    CameraViewBox camera_view = GetCameraView(&G_Universe.camera, game_viewport, camera_to_pixel);
    UniverseRenderer_Draw(&G_Universe, camera_view, camera_to_pixel);
}

int CreateNewWorld(bool auto_select)
{
    int index = Universe_CreateWorld(&G_Universe, COLOUR_GAME_PARCHMENT_RGBA, COLOUR_GAME_OLIVE_RGBA,
                                     camera_marker_colour, G_Universe.next_spawn,
                                     auto_select);
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
    bool pending = world_click_pending;
    world_click_pending = false;
    return pending;
}
