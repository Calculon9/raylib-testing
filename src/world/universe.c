/**********************************************************************************************
*
UNIVERSE MODULE
*
**********************************************************************************************/
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "system/systems.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"
#include "common/common.h"
#include "camera/camera.h"

Universe G_Universe = {0};
static bool world_camera_diagnostic_printed = false;

static Basis2d ResolveFrameBasis_UserInput(Vector2d requested_u, Vector2d requested_v)
{
    const float eps = 0.0001f;
    Basis2d result = IDENTITY_BASIS_2D;

    float u_mag = VectorMagnitude_2d(requested_u);
    float v_mag = VectorMagnitude_2d(requested_v);
    if (u_mag < eps || v_mag < eps)
    {
        LOG_WARN("Invalid coord-space basis magnitude. Falling back to identity basis. u=(%.3f,%.3f), v=(%.3f,%.3f)\n",
                 requested_u.x, requested_u.y, requested_v.x, requested_v.y);
        return result;
    }

    // Preserve authored basis magnitudes so the basis editor affects scale as well as direction.
    result.u = requested_u;
    result.v = requested_v;

    float det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    if (fabsf(det) < eps)
    {
        // If vectors are nearly collinear, rebuild v perpendicular to u while preserving v magnitude.
        Vector2d u_unit = VectorScale_2d(result.u, 1.0f / u_mag);
        result.v = (Vector2d){-u_unit.y * v_mag, u_unit.x * v_mag};
        det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    }

    if (det < 0.0f)
    {
        // Keep a consistent handedness for camera/grid transforms.
        result.v = VectorScale_2d(result.v, -1.0f);
    }

    return result;
}



// ---------------------------------------------------------------------------
void Universe_Init(Universe *u, Vector2d default_spawn, Vector2d default_new_world_resolution, float default_gravity)
{
    u->world_count = 0;
    u->selected_world_index = -1;

    for (int i = 0; i < UNIVERSE_MAX_WORLDS; i++)
    {
        u->camera_marker_ids[i] = 0;
        u->world_bounds_valid[i] = false;
        u->world_bounds_min[i] = ZERO_VECTOR_2D;
        u->world_bounds_max[i] = ZERO_VECTOR_2D;
    }

    u->next_spawn = default_spawn;
    u->next_resolution = default_new_world_resolution;
    u->next_basis_u = (Vector2d){1.0f, 0.0f};
    u->next_basis_v = (Vector2d){0.0f, 1.0f};
    u->next_gravity = default_gravity;

    // Default step: 15 % of the coord-space extent so successive spaces are offset but visible.
    u->spawn_step.x = (default_new_world_resolution.x > 0.0f) ? default_new_world_resolution.x * 0.15f : 5.0f;
    u->spawn_step.y = (default_new_world_resolution.y > 0.0f) ? default_new_world_resolution.y * 0.15f : 5.0f;
}

// ---------------------------------------------------------------------------
int Universe_CreateWorld(Universe *u, ColourRgba fill_colour, ColourRgba line_colour, ColourRgba camera_marker_colour, Vector2d world_center_in_universe,
                         bool auto_select)
{
    if (u->world_count >= UNIVERSE_MAX_WORLDS)
    {
        LOG_WARN("Cannot create world: reached max world count (%d).\n", UNIVERSE_MAX_WORLDS);
        return -1;
    }

    Vector2d requested_res = (u->next_resolution.x > 0.0f && u->next_resolution.y > 0.0f) ? u->next_resolution : (Vector2d){16.0f, 9.0f}; // sensible fallback

    Basis2d world_basis = ResolveFrameBasis_UserInput(u->next_basis_u, u->next_basis_v);
    u->next_basis_u = world_basis.u;
    u->next_basis_v = world_basis.v;

    GridSpace2d space_g = NewGridSpace2d(world_center_in_universe, requested_res, world_basis, fill_colour, line_colour);
    space_g.object.id = 0;
    space_g.object.flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
    space_g.object.collision_mask = FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE | FLAG_TYPE_WALL;
    space_g.object.entity_layer = FLAG_TYPE_WALL;

    int new_index = u->world_count;
    World2d *new_world = &u->worlds[new_index];
    // Temporary source pointer before CreateWorld copies GridSpace2d into world-owned storage.
    new_world->tunnel.source_frame = &space_g.space.frame;
    new_world->tunnel.destination_frame = u->camera.tunnel.source_frame;

    CreateWorld(space_g, u->next_gravity, new_world);

    // Rebind tunnel frames to persistent storage owned by the world and universe camera.
    new_world->tunnel.source_frame = &new_world->grid_space.space.frame;
    new_world->tunnel.destination_frame = &u->camera.frame;

    // Sync the world's frame origin to its anchored logical position in the universe
    new_world->grid_space.space.frame.origin_in_parent = world_center_in_universe;
    new_world->uni_coords_center = world_center_in_universe;
    
    // Keep world tunnel in stable world-local <-> universe space (camera zoom must not affect it).
    new_world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*new_world->tunnel.source_frame);
    new_world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(new_world->tunnel.source_to_dest_mtx);

    Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&new_world->grid_space.space.frame);
    Universe_SetWorldBounds(u, new_index, world_bounds.col1, world_bounds.col2);

    // Advance the default spawn so the next world is offset by default.
    u->next_spawn = VectorSum_2d(u->next_spawn, u->spawn_step);

    if (auto_select)
    {
        u->selected_world_index = new_index;
    }

    // Place marker in world-local space; world placement is applied by world transforms.
    Vector2d marker_local_coords = {0.5f, 0.5f};
    Newtonoid2d marker = CreateNewtonoid2d_Symmetric(4, 0.45f, camera_marker_colour, 1.0f,
                                                     marker_local_coords,
                                                     ZERO_VECTOR_2D, ZERO_VECTOR_2D);

    marker.entity_layer = FLAG_TYPE_EFFECT;
    marker.collision_mask = 0;
    marker.flags = FLAG_STATUS_ALIVE;
    marker.line_colour = camera_marker_colour;
    marker.fill_colour = camera_marker_colour;
    u->camera_marker_ids[new_index] = AddObjectToWorld(new_world, &marker, new_world->grid_space.object.id);

    u->world_count++;

    LOG_INFO("Universe_CreateWorld -> index=%d universe_pos=(%.1f,%.1f) res=(%.0fx%.0f)\n",
             new_index,
             new_world->grid_space.space.frame.origin_in_parent.x, new_world->grid_space.space.frame.origin_in_parent.y,
             requested_res.x, requested_res.y);

    return new_index;
}

// ---------------------------------------------------------------------------
bool Universe_SelectWorld(Universe *u, int index)
{
    if (index < 0 || index >= u->world_count)
        return false;

    u->selected_world_index = index;

    return true;
}

// ---------------------------------------------------------------------------
void Universe_SetWorldBounds(Universe *u, int index, Vector2d min_bound, Vector2d max_bound)
{
    if (u == NULL || index < 0 || index >= UNIVERSE_MAX_WORLDS)
        return;

    u->world_bounds_min[index] = min_bound;
    u->world_bounds_max[index] = max_bound;
    u->world_bounds_valid[index] = true;
}

// ---------------------------------------------------------------------------
int Universe_FindWorldAt(const Universe *u, Vector2d universe_point)
{
    if (u == NULL)
        return -1;

    for (int i = 0; i < u->world_count; i++)
    {
        if (!u->world_bounds_valid[i])
            continue;

        Vector2d min_bound = u->world_bounds_min[i];
        Vector2d max_bound = u->world_bounds_max[i];
        if (universe_point.x >= min_bound.x && universe_point.y >= min_bound.y &&
            universe_point.x < max_bound.x && universe_point.y < max_bound.y)
        {
            return i;
        }
    }

    return -1;
}

// ---------------------------------------------------------------------------
void Universe_Draw(Universe *u)
{
    for (int i = 0; i < u->world_count; i++)
    {
        World2d *w = &u->worlds[i];
        // Camera2d *draw_camera = &u->camera;
        // Vector2d draw_origin = WorldTopLeftInUniverse(w);
        // // Create composite camera for drawing the world in universe space.
        // Matrix3x3 world_to_universe_mtx = MatrixMultiply_3x3_3x3(w->camera.source_to_dest_mtx, u->camera.source_to_dest_mtx);
        // if (u->selected_world_index == i)
        // {
        //     draw_camera = &w->camera;
        //     draw_origin = ZERO_VECTOR_2D;
        // }

        // Universe view must be camera-stable: selection changes state, not rendering transform.
        DrawWorldRegion(w, &u->camera);
    }
}

// ---------------------------------------------------------------------------
bool Universe_ResolveClick(Universe *u, Vector2d universe_click, Vector2d *local_out)
{
    if (!u)
    {
        return false;
    }

    // Find which world (if any) contains the click point.
    int clicked_world = Universe_FindWorldAt(u, universe_click);
    if (clicked_world >= 0)
    {
        // Select the world immediately. Selection should occur regardless of the
        // local coordinate check, because the world bounds already guarantee the
        // click is inside the world.
        if (clicked_world != u->selected_world_index)
            Universe_SelectWorld(u, clicked_world);

        // Compute local coordinates relative to the world top-left for callers that need it.
        if (local_out)
        {
            World2d *w = &u->worlds[clicked_world];
            // Frame2d universe_space = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, u->resolution);
            // Frame2d world_space = w->grid_space.space.frame;
            Matrix3x3 universe_to_world_mtx = w->tunnel.dest_to_source_mtx; // Transform from universe space to world space
            *local_out = TransformCoordinates(universe_to_world_mtx, universe_click);
        }
        return true;
    }
    // No world hit - indicate no selection.
    return false;
}

// ---------------------------------------------------------------------------
int Universe_GetWorldCount(const Universe *u)
{
    return u->world_count;
}

int Universe_GetSelectedIndex(const Universe *u)
{
    return u->selected_world_index;
}

World2d *Universe_GetSelectedWorld(Universe *u)
{
    if (u->selected_world_index < 0 || u->selected_world_index >= u->world_count)
        return NULL;
    return &u->worlds[u->selected_world_index];
}

World2d *Universe_GetWorld(Universe *u, int index)
{
    if (index < 0 || index >= u->world_count)
        return NULL;
    return &u->worlds[index];
}

// ---------------------------------------------------------------------------
Camera2d *Universe_GetCamera(Universe *u)
{
    return &u->camera;
}
/**********************************************************************************************
 *
 *   Universe System Functions Definitions (Init, Update, Draw)
 *
 **********************************************************************************************/
#include "raylib.h"
#include "world/universe.h"
#include "world/world.h"
#include "world/universe.h"
#include "world/world.h"
#include "camera/camera.h"
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/cfont.h"
#include "ui/text_region.h"
#include "system/viewport_system.h"
#include "system/debug_overlay_system.h"
#include "system/drag_interaction.h"
#include "math/affine_space_ops.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int universe_grid_cells_x = 60;
static int universe_grid_cells_y = 60;
static float universe_grid_cell_size = 1.0f;
static int create_world_auto_select = 0;
static bool universe_camera_diagnostic_printed = false;
static bool universe_grid_aabb_diagnostic_printed = false;
ColourRgba camera_marker_colour = {255, 80, 80, 100};
Frame2d universe_frame = {0};      // Universe-space frame used for camera construction
FrameTunnel universe_tunnel = {0}; // Tunnel linking universe frame to viewport frame
static CameraController cam_ctrl = {0};
//----------------------------------------------------------------------------------
// Module Functions Declaration (forward declarations)
//----------------------------------------------------------------------------------
void DrawUniverseCameraMarker(Matrix3x3 M_cam_to_pixel);
void DrawUniverseGrid(CameraViewBox cam_view, Matrix3x3 M_cam_to_pixel);
static void ApplyWorldDragTransform(World2d *world, Vector2d new_universe_origin);

static void DrawUniverseGridLine(Vector2d start, Vector2d end, Matrix3x3 M_root_world_to_pixel, ColourRgba colour)
{
    Vector2d start_pixel = TransformCoordinates(M_root_world_to_pixel, start);
    Vector2d end_pixel = TransformCoordinates(M_root_world_to_pixel, end);

    DrawLineV((Vector2){(float)start_pixel.x, (float)start_pixel.y},
              (Vector2){(float)end_pixel.x, (float)end_pixel.y}, (Color){colour.r, colour.g, colour.b, colour.a});
}

static void SyncWorldStateFromSelection(void)
{
    G_UIState.selected_object = NULL;
    G_UIState.selected_cell = NULL;
    G_UIState.selected_cell_index = -1;
}

static Matrix3x3 BuildCameraToScreenMatrix(void)
{
    return MatrixMultiply_3x3_3x3(
        game_viewport.tunnel.source_to_dest_mtx,    // Left side: Step 2 (Final transformation to screen)
        G_Universe.camera.tunnel.source_to_dest_mtx // Right side: Step 1 (Initial camera projection)
    );
}

//----------------------------------------------------------------------------------
// Universe System Functions Definition
//----------------------------------------------------------------------------------
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

    // Camera matrices are rebuilt from zoom + rotation, so basis edits must
    // update those canonical fields instead of only mutating frame.basis.
    G_Universe.camera.rotation = VectorRadians_2d(basis.u);
    G_Universe.camera.zoom = u_mag;

    // Keep controller targets aligned so smoothing does not snap basis edits back.
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

    if (universe_grid_cell_size <= 0.0f)
    {
        universe_grid_cell_size = 1.0f;
    }

    Vector2d universe_resolution = {
        (float)universe_grid_cells_x * universe_grid_cell_size,
        (float)universe_grid_cells_y * universe_grid_cell_size};
    G_Universe.resolution = universe_resolution;

    Vector2d first_world_spawn = ZERO_VECTOR_2D;
    Universe_Init(&G_Universe, first_world_spawn, (Vector2d){4, 3}, gravity);

    // Establish universe_frame with centered spatial alignment
    // =========================================================================
    Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();

    // Create the master universe container frame centered perfectly at (0, 0)
    universe_frame = CreateFrame2d(IDENTITY_BASIS_2D, game_viewport_local_centre, universe_resolution);

    // Initialize the camera, pointing directly to its own frame member as the source space
    Vector2d default_lens_size = {
        game_viewport.frame.local_max.x - game_viewport.frame.local_min.x,
        game_viewport.frame.local_max.y - game_viewport.frame.local_min.y};
    G_Universe.camera = CreateCamera2d(&G_Universe.camera.frame, &game_viewport.frame);

    // Define the camera's lens coordinate frame data
    G_Universe.camera.frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, default_lens_size);

    // Set safe baseline logic defaults
    G_Universe.camera.source_focus_coords = ZERO_VECTOR_2D;
    G_Universe.camera.rotation = 0.0f;
    G_Universe.camera.zoom = 1.0f;

    // Forces matrix update and recalculates alignment safely
    SyncUniverseCameraToViewport();
    cam_ctrl = CreateCameraController(&G_Universe.camera);

    CreateNewWorld(false);
}

void UpdateUniverseSystem(int mouse_x, int mouse_y)
{
    Matrix3x3 M_root_world_to_pixel = BuildCameraToScreenMatrix();
    bool cursor_in_game_viewport = mouse_x >= game_viewport.pixel_origin.x &&
                                   mouse_x <= (game_viewport.pixel_origin.x + (game_viewport.pixel_u.x * game_viewport.resolution.x)) &&
                                   mouse_y >= game_viewport.pixel_origin.y &&
                                   mouse_y <= (game_viewport.pixel_origin.y + (game_viewport.pixel_v.y * game_viewport.resolution.y));

    if (!universe_camera_diagnostic_printed)
    {
        // Vector2d root_origin_pixel = TransformCoordinates(G_Universe.camera.tunnel.source_to_dest_mtx, ZERO_VECTOR_2D);
        Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();
        Vector2d game_viewport_pixel_centre = TransformCoordinates(M_root_world_to_pixel, game_viewport_local_centre);
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
    Matrix3x3 M_root_world_to_pixel = BuildCameraToScreenMatrix();
    Matrix3x3 M_pixel_to_world = MatrixInvert_3x3(M_root_world_to_pixel);
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    Vector2d mouse_pixel_coords = {(float)mouse_x, (float)mouse_y};

    if (cursor_in_game_viewport)
    {
        float wheel_move = GetMouseWheelMove();
        Vector2d pan_delta = ZERO_VECTOR_2D;
        
        // Define speed in units per second, scaled by delta time
        float current_speed = cam_ctrl.base_pan_speed / cam_ctrl.target_zoom; 

        if (IsKeyDown(KEY_UP))    pan_delta.y -= current_speed;
        if (IsKeyDown(KEY_DOWN))  pan_delta.y += current_speed;
        if (IsKeyDown(KEY_LEFT))  pan_delta.x -= current_speed;
        if (IsKeyDown(KEY_RIGHT)) pan_delta.x += current_speed;

        // Tell the controller to move the TARGET, not the actual camera
        if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
        {
            // Note: We use GetFrameTime() here so keyboard polling is smooth
            Controller_Pan(&cam_ctrl, pan_delta, frame_counter.delta_time);
        }

        // Tell the controller to change the target zoom
        if (IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (wheel_move > 0.0f)      cam_ctrl.target_zoom *= cam_ctrl.zoom_speed;
            else if (wheel_move < 0.0f) cam_ctrl.target_zoom /= cam_ctrl.zoom_speed;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT))
        {
            if (wheel_move > 0.0f)
                Controller_Rotate(&cam_ctrl, -0.05f); 
            else if (wheel_move < 0.0f)
                Controller_Rotate(&cam_ctrl, 0.05f);
        }
    }

    // Universe-level world dragging is only active when no world is selected.
    if (G_Universe.selected_world_index < 0)
    {
        if (cursor_in_game_viewport && IsMouseButtonDown((int)MOUSE_BUTTON_LEFT))
        {
            DragInteraction_UpdateButtonDown(game_drag_ctx, mouse_pixel_coords);

            if (game_drag_ctx->pointer_state.left_button_hold_ticks == 1)
            {
                Vector2d click_universe_coords = TransformCoordinates(M_pixel_to_world, mouse_pixel_coords);
                int world_index = Universe_FindWorldAt(&G_Universe, click_universe_coords);
                if (world_index >= 0)
                {
                    World2d *world = &G_Universe.worlds[world_index];
                    DragInteraction_BeginCapture(game_drag_ctx,
                                                DRAG_TARGET_WORLD_CONTAINER,
                                                world,
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
                    Vector2d initial_universe_coords = TransformCoordinates(M_pixel_to_world, game_drag_ctx->pointer_state.initial_pos);
                    Vector2d current_universe_coords = TransformCoordinates(M_pixel_to_world, game_drag_ctx->pointer_state.current_pos);
                    Vector2d drag_delta_universe = VectorSum_2d(current_universe_coords,
                                                                (Vector2d){-initial_universe_coords.x, -initial_universe_coords.y});
                    Vector2d new_universe_origin = VectorSum_2d(game_drag_ctx->target_anchor, drag_delta_universe);
                    ApplyWorldDragTransform(world, new_universe_origin);
                }
            }
        }
        else if (game_drag_ctx->pointer_state.left_button_hold_ticks > 0)
        {
            bool was_click = DragInteraction_IsClick(game_drag_ctx, 20, 5.0f);
            if (was_click && cursor_in_game_viewport)
            {
                Vector2d click_universe_coords = TransformCoordinates(M_pixel_to_world, mouse_pixel_coords);
                bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);

                if (!world_hit)
                {
                    G_Universe.selected_world_index = -1;
                }

                SyncWorldStateFromSelection();
            }

            DragInteraction_UpdateButtonUp(game_drag_ctx);
        }
    }
    else if (IsMouseButtonPressed((int)MOUSE_BUTTON_LEFT) && cursor_in_game_viewport)
    {
        Vector2d click_universe_coords = TransformCoordinates(M_pixel_to_world, mouse_pixel_coords);
        bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);

        if (!world_hit)
        {
            G_Universe.selected_world_index = -1;
        }

        SyncWorldStateFromSelection();
    }

    Controller_Update(&cam_ctrl);
}

static void ApplyWorldDragTransform(World2d *world, Vector2d new_universe_origin)
{
    if (!world)
    {
        return;
    }

    // Ensure tunnel frame pointers always reference canonical persistent frames.
    world->tunnel.source_frame = &world->grid_space.space.frame;
    world->tunnel.destination_frame = &G_Universe.camera.frame;

    world->grid_space.space.frame.origin_in_parent = new_universe_origin;
    world->uni_coords_center = new_universe_origin;

    // Keep tunnel transform in world-local <-> universe space.
    world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*world->tunnel.source_frame);
    world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(world->tunnel.source_to_dest_mtx);

    int world_index = (int)(world - G_Universe.worlds);
    if (world_index >= 0 && world_index < G_Universe.world_count)
    {
        Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&world->grid_space.space.frame);
        Universe_SetWorldBounds(&G_Universe, world_index, world_bounds.col1, world_bounds.col2);
    }
}

void DrawUniverse(void)
{
    Matrix3x3 M_cam_to_pixel = BuildCameraToScreenMatrix();
    CameraViewBox cam_view = GetCameraView(&G_Universe.camera, game_viewport, M_cam_to_pixel);
    // Draw universe grid background
    DrawUniverseGrid(cam_view, M_cam_to_pixel);

    Universe_Draw(&G_Universe);
    DrawUniverseCameraMarker(M_cam_to_pixel);
    DrawUniverseDebugOverlays(M_cam_to_pixel);
}

void DrawUniverseCameraMarker(Matrix3x3 M_cam_to_pixel)
{
    // Draw marker in screen-space at the active game viewport center.
    Vector2d camera_world_pos = G_Universe.camera.source_focus_coords;
    Vector2d pixel_origin = TransformCoordinates(M_cam_to_pixel, camera_world_pos);
    // Define the marker size in SCREEN PIXELS.
    // We adjust it by our camera zoom factor so it physically scales down/up with the world!
    float marker_pixel_size = 24.0f * (float)G_Universe.camera.zoom;
    float half_size = marker_pixel_size * 0.5f;

    // Calculate the top-left screen position
    Vector2 position = {
        (float)(pixel_origin.x - half_size),
        (float)(pixel_origin.y - half_size)};

    Vector2 size = {(float)marker_pixel_size, (float)marker_pixel_size};
    Color color = {camera_marker_colour.r, camera_marker_colour.g, camera_marker_colour.b, camera_marker_colour.a};

    // Draw the unwarped screen square centered on the camera position
    DrawRectangleV(position, size, color);
}

void DrawUniverseGrid(CameraViewBox cam_view, Matrix3x3 M_cam_to_pixel)
{
    Color grid_colour = {100, 100, 100, 100}; // Faint gray
    Color axis_x_colour = {230, 90, 90, 220};
    Color axis_y_colour = {90, 200, 255, 220};
    float grid_cell_size = universe_grid_cell_size;

    float uni_half_w = G_Universe.resolution.x * 0.5f;
    float uni_half_h = G_Universe.resolution.y * 0.5f;

    // Extract the min and max world coordinates of the camera view
    float lens_min_x = cam_view.origin.x;
    float lens_max_x = cam_view.origin.x + cam_view.dimensions.x;
    float lens_min_y = cam_view.origin.y;
    float lens_max_y = cam_view.origin.y + cam_view.dimensions.y;

    // Clamp boundary checks directly against global limits
    float world_min_x = fmaxf(lens_min_x, -uni_half_w);
    float world_max_x = fminf(lens_max_x, uni_half_w);
    float world_min_y = fmaxf(lens_min_y, -uni_half_h);
    float world_max_y = fminf(lens_max_y, uni_half_h);

    // Snap the loop limits to the grid lines baseline
    float start_x = floorf(world_min_x / grid_cell_size) * grid_cell_size;
    float start_y = floorf(world_min_y / grid_cell_size) * grid_cell_size;

    // =========================================================================
    // Generate lines using actual visible AABB bounds (prevents popping)
    // =========================================================================

    // Draw Vertical Lines
    for (float x = start_x; x <= world_max_x; x += grid_cell_size)
    {
        Vector2d line_start = {x, -uni_half_h};
        Vector2d line_end = {x, uni_half_h};

        Vector2d p_start = TransformCoordinates(M_cam_to_pixel, line_start);
        Vector2d p_end = TransformCoordinates(M_cam_to_pixel, line_end);

        DrawLineEx((Vector2){(float)p_start.x, (float)p_start.y}, (Vector2){(float)p_end.x, (float)p_end.y}, 1.0f, grid_colour);
    }

    // Draw Horizontal Lines
    for (float y = start_y; y <= world_max_y; y += grid_cell_size)
    {
        Vector2d line_start = {-uni_half_w, y};
        Vector2d line_end = {uni_half_w, y};

        Vector2d p_start = TransformCoordinates(M_cam_to_pixel, line_start);
        Vector2d p_end = TransformCoordinates(M_cam_to_pixel, line_end);

        DrawLineEx((Vector2){(float)p_start.x, (float)p_start.y}, (Vector2){(float)p_end.x, (float)p_end.y}, 1.0f, grid_colour);
    }

    // Draw Primary Target Origin Axes
    Vector2d x_axis_start_px = TransformCoordinates(M_cam_to_pixel, (Vector2d){world_min_x, 0.0f});
    Vector2d x_axis_end_px = TransformCoordinates(M_cam_to_pixel, (Vector2d){world_max_x, 0.0f});
    Vector2d y_axis_start_px = TransformCoordinates(M_cam_to_pixel, (Vector2d){0.0f, world_min_y});
    Vector2d y_axis_end_px = TransformCoordinates(M_cam_to_pixel, (Vector2d){0.0f, world_max_y});

    DrawLineEx((Vector2){(float)x_axis_start_px.x, (float)x_axis_start_px.y}, (Vector2){(float)x_axis_end_px.x, (float)x_axis_end_px.y}, 2.5f, axis_x_colour);
    DrawLineEx((Vector2){(float)y_axis_start_px.x, (float)y_axis_start_px.y}, (Vector2){(float)y_axis_end_px.x, (float)y_axis_end_px.y}, 2.5f, axis_y_colour);

    // --- DEBUG LABELS SECTOR ---
    if (!IsDebugOverlayEnabled(DEBUG_OVERLAY_UNIVERSE_GRID_LABELS))
        return;

    // Check zoom level text density limits
    Vector2d p00 = TransformCoordinates(M_cam_to_pixel, (Vector2d){0.0f, 0.0f});
    Vector2d p10 = TransformCoordinates(M_cam_to_pixel, (Vector2d){grid_cell_size, 0.0f});
    float cell_px_w = (float)VectorMagnitude_2d((Vector2d){p10.x - p00.x, p10.y - p00.y});
    if (cell_px_w < 40.0f)
        return;

    Color text_colour = (Color){210, 210, 210, 220};

    for (float y = start_y; y < world_max_y; y += grid_cell_size)
    {
        for (float x = start_x; x < world_max_x; x += grid_cell_size)
        {
            Vector2d cell_origin = {x, y};
            Vector2d cell_pixel = TransformCoordinates(M_cam_to_pixel, cell_origin);

            // Calculate grid indices
            int ix = (int)floorf((x + uni_half_w) / grid_cell_size);
            int iy = (int)floorf((y + uni_half_h) / grid_cell_size);

            if (ix < 0) ix = 0;
            if (ix >= universe_grid_cells_x) ix = universe_grid_cells_x - 1;
            if (iy < 0) iy = 0;
            if (iy >= universe_grid_cells_y) iy = universe_grid_cells_y - 1;

            int cell_index = (iy * universe_grid_cells_x) + ix;

            const char *display_text = TextFormat("%d\n(%.0f,%.0f)", cell_index, cell_origin.x, cell_origin.y);
            DrawTextEx(font, display_text, (Vector2){(float)cell_pixel.x + 2, (float)cell_pixel.y + 2}, 18, 1, text_colour);
        }
    }
}

int CreateNewWorld(bool auto_select)
{
    // Vector2d game_viewport_local_center = ResolveGameViewportCenter();
    int index = Universe_CreateWorld(&G_Universe, WHITE_RGBA, LIGHTGRAY_RGBA, camera_marker_colour, G_Universe.next_spawn,
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
World2d *GetWorldByIndex(int i) { return Universe_GetWorld(&G_Universe, i); }

Vector2d *GetNextWorldSpawnOriginPtr(void) { return &G_Universe.next_spawn; }
Vector2d *GetNextWorldResolutionPtr(void) { return &G_Universe.next_resolution; }
Vector2d *GetNextWorldBasisUPtr(void) { return &G_Universe.next_basis_u; }
Vector2d *GetNextWorldBasisVPtr(void) { return &G_Universe.next_basis_v; }
float *GetNextWorldGravityPtr(void) { return &G_Universe.next_gravity; }

