/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 *   Universe System Functions Definitions (Init, Update, Draw)
 *
 **********************************************************************************************/
#include "raylib.h"
#include "system/universe_system.h"
#include "system/world_system.h"
#include "world/universe.h"
#include "world/world.h"
#include "camera/camera.h"
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/cfont.h"
#include "ui/text_region.h"
#include "system/viewport_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int universe_grid_cells_x = 60;
static int universe_grid_cells_y = 60;
static float universe_grid_cell_size = 1.0f;
static int create_world_auto_select = 0;
static bool universe_grid_debug_labels_enabled = true;
static bool coordinate_debug_overlay_enabled = false;
static bool universe_camera_diagnostic_printed = false;
static bool universe_grid_aabb_diagnostic_printed = false;
ColourRgba camera_marker_colour = {255, 80, 80, 100};
Frame2d universe_frame = {0};      // Universe-space frame used for camera construction
FrameTunnel universe_tunnel = {0}; // Tunnel linking universe frame to viewport frame
//----------------------------------------------------------------------------------
// Module Functions Declaration (forward declarations)
//----------------------------------------------------------------------------------
void DrawUniverseCameraMarker(Matrix3x3 M_root_world_to_pixel);
void DrawUniverseGrid(Matrix3x3 M_root_world_to_pixel);
void DrawCoordinateDebugOverlay(Matrix3x3 M_root_world_to_pixel);

static void DrawUniverseGridLine(Vector2d start, Vector2d end, Matrix3x3 M_root_world_to_pixel, ColourRgba colour)
{
    Vector2d start_pixel = TransformCoordinates(M_root_world_to_pixel, start);
    Vector2d end_pixel = TransformCoordinates(M_root_world_to_pixel, end);

    DrawLineV((Vector2){(float)start_pixel.x, (float)start_pixel.y},
              (Vector2){(float)end_pixel.x, (float)end_pixel.y}, (Color){colour.r, colour.g, colour.b, colour.a});
}

static void SyncWorldStateFromSelection(void)
{
    World2d *w = Universe_GetSelectedWorld(&G_Universe);
    G_WorldState.world = w;
    G_WorldState.entity_world_index_registry = w ? &w->entity_world_index_registry : NULL;
    G_WorldState.collisions = w ? &w->collisions : NULL;
    G_WorldState.selected_object = NULL;
    G_WorldState.selected_cell = NULL;
    G_WorldState.selected_cell_index = -1;
}

static Matrix3x3 BuildRootWorldToPixelMatrix(void)
{
    return MatrixMultiply_3x3_3x3(
        game_viewport_tunnel.source_to_dest_mtx,    // Left side: Step 2 (Final transformation to screen)
        G_Universe.camera.tunnel.source_to_dest_mtx // Right side: Step 1 (Initial camera projection)
    );
}

//----------------------------------------------------------------------------------
// Universe System Functions Definition
//----------------------------------------------------------------------------------

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
    G_Universe.camera = CreateCamera2d(&G_Universe.camera.frame, &game_viewport_frame);

    // Define the camera's lens coordinate frame data safely
    G_Universe.camera.frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, universe_resolution);

    // Set safe baseline logic defaults
    G_Universe.camera.source_focus_coords = ZERO_VECTOR_2D;
    G_Universe.camera.target_source_focus_coords = ZERO_VECTOR_2D;
    G_Universe.camera.rotation = 0.0f;
    G_Universe.camera.zoom = 1.0f;
    G_Universe.camera.target_zoom = 1.0f;

    // Forces matrix update and recalculates alignment safely
    SyncUniverseCameraToViewport();

    CreateNewWorld(false);
}

void SyncUniverseCameraToViewport(void)
{
    Camera_SetDestinationFrame(&G_Universe.camera, &game_viewport_frame);
    // Double-check that the tunnel's source is explicitly bound to our internal frame
    G_Universe.camera.tunnel.source_frame = &G_Universe.camera.frame;
    UpdateCameraFull(&G_Universe.camera);
}

void UpdateUniverseSystem(int mouse_x, int mouse_y)
{
    Matrix3x3 M_root_world_to_pixel = BuildRootWorldToPixelMatrix();
    bool cursor_in_game_viewport = mouse_x >= game_viewport_pixel_origin.x &&
                                   mouse_x <= (game_viewport_pixel_origin.x + (game_viewport_pixel_u.x * game_viewport_resolution.x)) &&
                                   mouse_y >= game_viewport_pixel_origin.y &&
                                   mouse_y <= (game_viewport_pixel_origin.y + (game_viewport_pixel_v.y * game_viewport_resolution.y));

    if (!universe_camera_diagnostic_printed)
    {
        // Vector2d root_origin_pixel = TransformCoordinates(G_Universe.camera.tunnel.source_to_dest_mtx, ZERO_VECTOR_2D);
        Vector2d game_viewport_local_centre = ResolveGameViewportLocalCenter();
        Vector2d game_viewport_pixel_centre = TransformCoordinates(M_root_world_to_pixel, game_viewport_local_centre);
        LOG_INFO("[ROOT CAMERA DIAG] dest_origin=(%.2f, %.2f) src_focus=(%.2f, %.2f) viewport_px_center=(%.2f, %.2f) viewport_local_center=(%.2f, %.2f)\n",
                 G_Universe.camera.tunnel.destination_frame->origin_in_parent.x,
                 G_Universe.camera.tunnel.destination_frame->origin_in_parent.y,
                 G_Universe.camera.source_focus_coords.x,
                 G_Universe.camera.source_focus_coords.y,
                 game_viewport_pixel_centre.x,
                 game_viewport_pixel_centre.y,
                 game_viewport_local_centre.x,
                 game_viewport_local_centre.y);
        universe_camera_diagnostic_printed = true;
    }

    UpdateUniverseInput(mouse_x, mouse_y, cursor_in_game_viewport);
}

void UpdateUniverseInput(int mouse_x, int mouse_y, bool cursor_in_game_viewport)
{
    Matrix3x3 M_root_world_to_pixel = BuildRootWorldToPixelMatrix();
    Matrix3x3 M_pixel_to_world = MatrixInvert_3x3(M_root_world_to_pixel);

    // Universe camera controls: arrow keys for panning, Ctrl +/- for zooming
    // Only pan/zoom when cursor is in the viewport region
    if (cursor_in_game_viewport)
    {
        float wheel_move = GetMouseWheelMove();
        Vector2d pan_delta = ZERO_VECTOR_2D;
        if (IsKeyDown(KEY_UP))
            pan_delta.y -= 0.5f;
        if (IsKeyDown(KEY_DOWN))
            pan_delta.y += 0.5f;
        if (IsKeyDown(KEY_LEFT))
            pan_delta.x -= 0.5f;
        if (IsKeyDown(KEY_RIGHT))
            pan_delta.x += 0.5f;

        if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
            PanCamera(&G_Universe.camera, VectorScale_2d(pan_delta, -1.0f));

        // Zoom with Ctrl +/-
        // if (IsKeyDown(KEY_LEFT_CONTROL))
        // {
        //     if (IsKeyPressed(KEY_EQUAL))
        //         ZoomCamera(&G_Universe.camera, 1.1f);
        //     else if (IsKeyPressed(KEY_MINUS))
        //         ZoomCamera(&G_Universe.camera, 1.0f / 1.1f);
        // }
        if (IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (wheel_move > 0.0f)
                ZoomCamera(&G_Universe.camera, 1.1f);
            else if (wheel_move < 0.0f)
                ZoomCamera(&G_Universe.camera, 1.0f / 1.1f);
        }

        if (IsKeyDown(KEY_LEFT_SHIFT))
        {
            if (wheel_move > 0.0f)
                RotateCamera(&G_Universe.camera, -0.05f); // Scaled down for smoothness + inverted
            else if (wheel_move < 0.0f)
                RotateCamera(&G_Universe.camera, 0.05f);
            // if (IsKeyPressed(KEY_EQUAL))
            //     RotateCamera(&G_Universe.camera, 0.25);
            // else if (IsKeyPressed(KEY_MINUS))
            //     RotateCamera(&G_Universe.camera, -0.25);
        }
    }
    // Click to select/deselect worlds
    if (IsMouseButtonPressed((int)MOUSE_BUTTON_LEFT) && cursor_in_game_viewport)
    {
        Vector2d click_pixel_coords = {mouse_x, mouse_y};
        Vector2d click_universe_coords = TransformCoordinates(M_pixel_to_world, click_pixel_coords);
        bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);

        // If no world was hit, deselect and reset camera offset so all worlds are visible
        if (!world_hit)
        {
            G_Universe.selected_world_index = -1;
            // G_Universe.camera_offset = ZERO_VECTOR_2D;
        }

        SyncWorldStateFromSelection();
    }

    if (IsKeyPressed(KEY_F6))
    {
        universe_grid_debug_labels_enabled = !universe_grid_debug_labels_enabled;
        printf("[Universe] Grid debug labels: %s\n", universe_grid_debug_labels_enabled ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F11))
    {
        coordinate_debug_overlay_enabled = !coordinate_debug_overlay_enabled;
        printf("[Debug] Coordinate overlay: %s\n", coordinate_debug_overlay_enabled ? "ON" : "OFF");
    }

    UpdateCameraSmoothingTick(&G_Universe.camera);
}

void DrawUniverse(void)
{
    Matrix3x3 M_root_world_to_pixel = BuildRootWorldToPixelMatrix();
    // Draw universe grid background
    DrawUniverseGrid(M_root_world_to_pixel);

    Universe_Draw(&G_Universe);
    DrawUniverseCameraMarker(M_root_world_to_pixel);
    DrawCoordinateDebugOverlay(M_root_world_to_pixel);
}

void DrawCoordinateDebugOverlay(Matrix3x3 M_root_world_to_pixel)
{
    if (!coordinate_debug_overlay_enabled)
    {
        return;
    }

    // 1. Fetch raw mouse position from the hardware OS layer
    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    Vector2d pixel = {(float)mouse_x, (float)mouse_y};

    bool cursor_in_game_viewport = mouse_x >= game_viewport_pixel_origin.x && mouse_x <= game_viewport_pixel_end.x &&
                                   mouse_y >= game_viewport_pixel_origin.y && mouse_y <= game_viewport_pixel_end.y;

    //  Unproject Screen Pixels completely into Universe (World) Coordinates
    // =========================================================================
    Matrix3x3 M_pixel_to_world = MatrixInvert_3x3(M_root_world_to_pixel);
    Vector2d parent_local = TransformCoordinates(M_pixel_to_world, pixel);

    int selected_index = G_Universe.selected_world_index;
    int hovered_world_index = Universe_FindWorldAt(&G_Universe, parent_local);
    int target_world_index = (hovered_world_index >= 0) ? hovered_world_index : selected_index;

    bool has_child_local = false;
    Vector2d child_origin_in_parent = ZERO_VECTOR_2D;
    Vector2d child_local = ZERO_VECTOR_2D;
    int world_cell_index = -1;

    if (target_world_index >= 0 && target_world_index < G_Universe.world_count)
    {
        World2d *w = &G_Universe.worlds[target_world_index];
        Vector2d res = {(float)w->grid_space.space.columns, (float)w->grid_space.space.rows};

        // Unproject from Universe Space down into the targeted World Space
        // Calculate the world's master positioning matrix within the universe,
        // then invert it to transform the universe cursor position cleanly into sub-grid units.
        Matrix3x3 M_world_to_universe = w->camera.tunnel.source_to_dest_mtx;
        Matrix3x3 M_universe_to_world = w->camera.tunnel.dest_to_source_mtx; // Inverse of the above

        has_child_local = true;
        child_local = TransformCoordinates(M_universe_to_world, parent_local);

        // Track structural frame origins using their persistent coordinate values
        child_origin_in_parent.x = w->grid_space.space.frame.origin_in_parent.x + (res.x * 0.5f);
        child_origin_in_parent.y = w->grid_space.space.frame.origin_in_parent.y + (res.y * 0.5f);

        // Bounding box index validation
        if (child_local.x >= 0.0f && child_local.y >= 0.0f &&
            child_local.x < res.x && child_local.y < res.y)
        {
            int cell_x = (int)floorf(child_local.x);
            int cell_y = (int)floorf(child_local.y);
            world_cell_index = (cell_y * (int)res.x) + cell_x;
        }
    }

    // DRAW OVERLAY BOX (Drawn using absolute pixel units inside the game viewport)
    // =========================================================================
    const int panel_x = (int)game_viewport_pixel_origin.x + 6;
    const int panel_y = (int)game_viewport_pixel_origin.y + 6;
    const int panel_w = 660;
    const int panel_h = 188;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){12, 16, 24, 210});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){210, 230, 255, 180});

    char line1[256], line2[256], line3[256], line4[256], line5[256], line6[256];

    snprintf(line1, sizeof(line1), "Cursor px: (%.1f, %.1f) [%s]", pixel.x, pixel.y, cursor_in_game_viewport ? "in viewport" : "outside viewport");
    snprintf(line2, sizeof(line2), "Parent coords (Universe): (%.3f, %.3f)", parent_local.x, parent_local.y);

    if (has_child_local)
    {
        snprintf(line3, sizeof(line3), "Child local [world %d]: (%.3f, %.3f)", target_world_index, child_local.x, child_local.y);
        snprintf(line4, sizeof(line4), "Child origin in parent: (%.3f, %.3f)", child_origin_in_parent.x, child_origin_in_parent.y);
        snprintf(line5, sizeof(line5), "Child cell index: %d", world_cell_index);
    }
    else
    {
        snprintf(line3, sizeof(line3), "Child local: n/a (no selected/hovered world)");
        snprintf(line4, sizeof(line4), "Child origin in parent: n/a");
        snprintf(line5, sizeof(line5), "Child cell index: n/a");
    }

    snprintf(line6, sizeof(line6), "Selected world: %d | Hovered world: %d | Toggle: F11", selected_index, hovered_world_index);

    Bitmap_Font overlay_font = FONT_BASIC;

    DrawTextCustom(line1, (Vector2d){(float)panel_x + 10, (float)panel_y + 10}, overlay_font.scale, overlay_font, (ColourRgba){240, 246, 255, 230});
    DrawTextCustom(line2, (Vector2d){(float)panel_x + 10, (float)panel_y + 38}, overlay_font.scale, overlay_font, (ColourRgba){240, 246, 255, 230});
    DrawTextCustom(line3, (Vector2d){(float)panel_x + 10, (float)panel_y + 66}, overlay_font.scale, overlay_font, (ColourRgba){240, 246, 255, 230});
    DrawTextCustom(line4, (Vector2d){(float)panel_x + 10, (float)panel_y + 94}, overlay_font.scale, overlay_font, (ColourRgba){190, 220, 255, 230});
    DrawTextCustom(line5, (Vector2d){(float)panel_x + 10, (float)panel_y + 122}, overlay_font.scale, overlay_font, (ColourRgba){190, 220, 255, 230});
    DrawTextCustom(line6, (Vector2d){(float)panel_x + 10, (float)panel_y + 150}, overlay_font.scale, overlay_font, (ColourRgba){190, 220, 255, 230});
}

void DrawUniverseCameraMarker(Matrix3x3 M_root_world_to_pixel)
{
    // Draw marker in screen-space at the active game viewport center.
    Vector2d camera_world_pos = G_Universe.camera.source_focus_coords;
    Vector2d pixel_origin = TransformCoordinates(M_root_world_to_pixel, camera_world_pos);
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

void DrawUniverseGrid(Matrix3x3 M_root_world_to_pixel)
{
    Color grid_colour = {100, 100, 100, 100}; // Faint gray
    Color axis_x_colour = {230, 90, 90, 220};
    Color axis_y_colour = {90, 200, 255, 220};
    float grid_cell_size = universe_grid_cell_size;

    // Invert cascade matrix to convert viewport pixel corners to local space
    Matrix3x3 M_pixel_to_world = MatrixInvert_3x3(M_root_world_to_pixel);

    Vector2d viewport_coords[] = {
        TransformCoordinates(M_pixel_to_world, game_viewport_pixel_origin),
        TransformCoordinates(M_pixel_to_world, (Vector2d){(float)game_viewport_pixel_end.x, game_viewport_pixel_origin.y}),
        TransformCoordinates(M_pixel_to_world, (Vector2d){game_viewport_pixel_origin.x, (float)game_viewport_pixel_end.y}),
        TransformCoordinates(M_pixel_to_world, game_viewport_pixel_end)};

    // Calculate bounds from spatial matrices
    Matrix2x2 game_aabb = CalcAABBCoords_Tight(viewport_coords, 4, ZERO_VECTOR_2D);

    float uni_half_w = G_Universe.resolution.x * 0.5f;
    float uni_half_h = G_Universe.resolution.y * 0.5f;

    // Clamp boundary checks directly against global limits
    float world_min_x = fmaxf(game_aabb.col1.x, -uni_half_w);
    float world_max_x = fminf(game_aabb.col2.x, uni_half_w);
    float world_min_y = fmaxf(game_aabb.col1.y, -uni_half_h);
    float world_max_y = fminf(game_aabb.col2.y, uni_half_h);

    // Snap the loop limits to the grid lines baseline
    float start_x = floorf(world_min_x / grid_cell_size) * grid_cell_size;
    float start_y = floorf(world_min_y / grid_cell_size) * grid_cell_size;

    // =========================================================================
    // FIX 1: Generate lines using actual visible AABB bounds (prevents popping)
    // =========================================================================

    // Draw Vertical Lines
    for (float x = start_x; x <= world_max_x; x += grid_cell_size)
    {
        Vector2d line_start = {x, -uni_half_h};
        Vector2d line_end = {x, uni_half_h};

        Vector2d p_start = TransformCoordinates(M_root_world_to_pixel, line_start);
        Vector2d p_end = TransformCoordinates(M_root_world_to_pixel, line_end);

        DrawLineEx((Vector2){(float)p_start.x, (float)p_start.y}, (Vector2){(float)p_end.x, (float)p_end.y}, 1.0f, grid_colour);
    }

    // Draw Horizontal Lines
    for (float y = start_y; y <= world_max_y; y += grid_cell_size)
    {
        Vector2d line_start = {-uni_half_w, y};
        Vector2d line_end = {uni_half_w, y};

        Vector2d p_start = TransformCoordinates(M_root_world_to_pixel, line_start);
        Vector2d p_end = TransformCoordinates(M_root_world_to_pixel, line_end);

        DrawLineEx((Vector2){(float)p_start.x, (float)p_start.y}, (Vector2){(float)p_end.x, (float)p_end.y}, 1.0f, grid_colour);
    }

    // Draw Primary Target Origin Axes
    Vector2d x_axis_start_px = TransformCoordinates(M_root_world_to_pixel, (Vector2d){world_min_x, 0.0f});
    Vector2d x_axis_end_px = TransformCoordinates(M_root_world_to_pixel, (Vector2d){world_max_x, 0.0f});
    Vector2d y_axis_start_px = TransformCoordinates(M_root_world_to_pixel, (Vector2d){0.0f, world_min_y});
    Vector2d y_axis_end_px = TransformCoordinates(M_root_world_to_pixel, (Vector2d){0.0f, world_max_y});

    DrawLineEx((Vector2){(float)x_axis_start_px.x, (float)x_axis_start_px.y}, (Vector2){(float)x_axis_end_px.x, (float)x_axis_end_px.y}, 2.5f, axis_x_colour);
    DrawLineEx((Vector2){(float)y_axis_start_px.x, (float)y_axis_start_px.y}, (Vector2){(float)y_axis_end_px.x, (float)y_axis_end_px.y}, 2.5f, axis_y_colour);

    // --- DEBUG LABELS SECTOR ---
    if (!universe_grid_debug_labels_enabled)
        return;

    // Check zoom level text density limits
    Vector2d p00 = TransformCoordinates(M_root_world_to_pixel, (Vector2d){0.0f, 0.0f});
    Vector2d p10 = TransformCoordinates(M_root_world_to_pixel, (Vector2d){grid_cell_size, 0.0f});
    float cell_px_w = (float)VectorMagnitude_2d((Vector2d){p10.x - p00.x, p10.y - p00.y});
    if (cell_px_w < 40.0f)
        return;

    Color text_colour = (Color){210, 210, 210, 220};

    for (float y = start_y; y < world_max_y; y += grid_cell_size)
    {
        for (float x = start_x; x < world_max_x; x += grid_cell_size)
        {
            Vector2d cell_origin = {x, y};
            Vector2d cell_pixel = TransformCoordinates(M_root_world_to_pixel, cell_origin);

            // Screen boundary check
            if (cell_pixel.x < game_viewport_pixel_origin.x - 100 || cell_pixel.x > game_viewport_pixel_end.x + 100 ||
                cell_pixel.y < game_viewport_pixel_origin.y - 100 || cell_pixel.y > game_viewport_pixel_end.y + 100)
            {
                continue;
            }

            int ix = (int)floorf((x + uni_half_w) / grid_cell_size);
            int iy = (int)floorf((y + uni_half_h) / grid_cell_size);

            if (ix < 0)
                ix = 0;
            if (ix >= universe_grid_cells_x)
                ix = universe_grid_cells_x - 1;
            if (iy < 0)
                iy = 0;
            if (iy >= universe_grid_cells_y)
                iy = universe_grid_cells_y - 1;

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
                                     &G_WorldState, auto_select);
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
