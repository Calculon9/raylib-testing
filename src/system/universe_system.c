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

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int universe_grid_cells_x = 120;
static int universe_grid_cells_y = 70;
static float universe_grid_cell_size = 1.0f;

//----------------------------------------------------------------------------------
// Module Functions Declaration (forward declarations)
//----------------------------------------------------------------------------------
void DrawUniverseCameraMarker(void);
void DrawUniverseGrid(void);

//----------------------------------------------------------------------------------
// Universe System Functions Definition
//----------------------------------------------------------------------------------

void InitUniverseSystem(void)
{
    // Initialize universe with independent universe coordinate space.
    // Universe dimensions are configured in cells and are independent of panels.
    extern Vector2d world_resolution;
    extern Vector2d universe_viewport_origin;
    extern Vector2d universe_viewport_u;
    extern Vector2d universe_viewport_v;
    extern float gravity;

    if (universe_grid_cell_size <= 0.0f)
        universe_grid_cell_size = 1.0f;

    Vector2d universe_resolution = {
        (float)universe_grid_cells_x * universe_grid_cell_size,
        (float)universe_grid_cells_y * universe_grid_cell_size
    };

    // Spawn first world centered in universe so startup composition is stable.
    Vector2d first_world_spawn = {
        (universe_resolution.x - world_resolution.x) * 0.5f,
        (universe_resolution.y - world_resolution.y) * 0.5f
    };

    Universe_Init(&G_Universe, first_world_spawn, world_resolution, universe_resolution, gravity);

    // Universe camera outputs into the game viewport (screen-space basis/origin).
    G_Universe.camera.destination_basis = (Basis2d){universe_viewport_u, universe_viewport_v};
    G_Universe.camera.destination_origin_coords = universe_viewport_origin;
    UpdateCameraTransforms(&G_Universe.camera);
}

void SetUniverseGridCellCounts(int cells_x, int cells_y)
{
    if (cells_x < 10)
        cells_x = 10;
    if (cells_y < 10)
        cells_y = 10;

    universe_grid_cells_x = cells_x;
    universe_grid_cells_y = cells_y;
}

void SetUniverseGridCellSize(float cell_size)
{
    if (cell_size < 0.1f)
        cell_size = 0.1f;
    if (cell_size > 100.0f)
        cell_size = 100.0f;

    universe_grid_cell_size = cell_size;
}

void UpdateUniverseInput(int mouse_x, int mouse_y, bool cursor_in_viewport)
{
    // Universe camera controls: arrow keys for panning, Ctrl +/- for zooming
    // Only pan/zoom when cursor is in the viewport region
    if (cursor_in_viewport)
    {
        Vector2d pan_delta = ZERO_VECTOR_2D;
        if (IsKeyDown(KEY_UP))
            pan_delta.y += 0.5f;
        if (IsKeyDown(KEY_DOWN))
            pan_delta.y -= 0.5f;
        if (IsKeyDown(KEY_LEFT))
            pan_delta.x += 0.5f;
        if (IsKeyDown(KEY_RIGHT))
            pan_delta.x -= 0.5f;

        if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
            Universe_PanCamera(&G_Universe, pan_delta);

        // Zoom with Ctrl +/-
        if (IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (IsKeyPressed(KEY_EQUAL))
                Universe_ZoomCamera(&G_Universe, 1.1f);
            else if (IsKeyPressed(KEY_MINUS))
                Universe_ZoomCamera(&G_Universe, 0.9f);
        }
    }

    // Click to select/deselect worlds
    if (IsMouseButtonPressed((int)MOUSE_BUTTON_LEFT) && cursor_in_viewport)
    {
        extern Vector2d world_origin;
        Vector2d click_pixel_coords = {mouse_x, mouse_y};
        // Use universe camera to transform pixel to universe space
        Vector2d click_universe_coords = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, click_pixel_coords);
        Vector2d local_coords = {0};
        bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, world_origin, &local_coords);

        // If no world was hit, deselect and reset camera offset so all worlds are visible
        if (!world_hit)
        {
            G_Universe.selected_world_index = -1;
            G_Universe.camera_offset = ZERO_VECTOR_2D;
        }
    }
}

void DrawUniverse(void)
{
    extern Camera2d camera_world;
    extern Vector2d world_origin;
    
    // Draw universe grid background
    DrawUniverseGrid();
    
    Universe_Draw(&G_Universe, &camera_world);
    DrawUniverseCameraMarker();
}

void DrawUniverseCameraMarker(void)
{
    extern ColourRgba camera_world_marker_colour;
    
    // Draw universe camera marker always so we can see where the universe camera is
    Vector2d camera_pos = G_Universe.camera.camera_coords;
    Vector2d marker_half_size = {0.25f, 0.25f}; // Half-size for the marker
    Vector2d marker_min = VectorSum_2d(camera_pos, VectorScale_2d(marker_half_size, -1.0f));
    Vector2d marker_max = VectorSum_2d(camera_pos, marker_half_size);

    // Transform to screen coordinates
    Vector2d marker_min_screen = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, marker_min);
    Vector2d marker_max_screen = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, marker_max);

    Vector2d size_2d = {marker_max_screen.x - marker_min_screen.x, marker_max_screen.y - marker_min_screen.y};

    // Convert to raylib types
    Vector2 position = {marker_min_screen.x, marker_min_screen.y};
    Vector2 size = {size_2d.x, size_2d.y};
    Color color = {camera_world_marker_colour.r, camera_world_marker_colour.g, camera_world_marker_colour.b, camera_world_marker_colour.a};

    // Draw a small square at the universe camera position
    DrawRectangleV(position, size, color);
}

void DrawUniverseGrid(void)
{
    // Grid lines for universe visualization - independent of world size
    // Grid cells are a fixed size in logical units
    ColourRgba grid_colour = {100, 100, 100, 130};  // Very faint, highly transparent gray
    float grid_cell_size = universe_grid_cell_size;

    Vector2d universe_min = {0, 0};
    Vector2d universe_max = G_Universe.resolution;
    
    // Draw vertical lines (along Y)
    for (float x = 0; x <= universe_max.x; x += grid_cell_size)
    {
        Vector2d line_start = {x, universe_min.y};
        Vector2d line_end = {x, universe_max.y};
        
        Vector2d line_start_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, line_start);
        Vector2d line_end_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, line_end);
        
        DrawLineV((Vector2){line_start_pixel.x, line_start_pixel.y},
                  (Vector2){line_end_pixel.x, line_end_pixel.y},
                  (Color){grid_colour.r, grid_colour.g, grid_colour.b, grid_colour.a});
    }
    
    // Draw horizontal lines (along X)
    for (float y = 0; y <= universe_max.y; y += grid_cell_size)
    {
        Vector2d line_start = {universe_min.x, y};
        Vector2d line_end = {universe_max.x, y};
        
        Vector2d line_start_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, line_start);
        Vector2d line_end_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, line_end);
        
        DrawLineV((Vector2){line_start_pixel.x, line_start_pixel.y},
                  (Vector2){line_end_pixel.x, line_end_pixel.y},
                  (Color){grid_colour.r, grid_colour.g, grid_colour.b, grid_colour.a});
    }
}
