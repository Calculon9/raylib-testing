/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 *   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
 *
 *   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
 *
 *   This software is provided "as-is", without any express or implied warranty. In no event
 *   will the authors be held liable for any damages arising from the use of this software.
 *
 *   Permission is granted to anyone to use this software for any purpose, including commercial
 *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *     wrote the original software. If you use this software in a product, an acknowledgment
 *     in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *     as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
 *
 **********************************************************************************************/
#include <stdint.h>
#include "raylib.h"
#include "system/utility_system.h"
#include "system/world_system.h"
#include "system/universe_system.h"
#include "system/systems.h"
#include "system/job_system.h"
#include "physics/physics.h"
#include "common/common.h"
#include "world/world.h"
#include "world/universe.h"
#include "camera/camera.h"
#include <math/helpers.h>
// #include "screens.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
WorldState G_WorldState = (WorldState){0};
// ----------WORLD SCREEN----------
static int finishScreen = 0;
static Newtonoid2d *selectedObject = NULL; // Pointer to the currently selected object (if any) for displaying its properties in the panel
static Cell *selectedCell = NULL;          // Pointer to the currently selected cell (if any) for displaying its properties in the panel
// Visual Properties
static ColourRgba world_text_colour = BROWN_1_RGBA_4; //{55, 97, 0, 200};
static ColourRgba world_fill_colour = WHITE_RGBA;     //{48, 104, 68, 70}; // MAROON_RGBA;// {150, 255, 220,180};//DARKGREEN_RGBA;
static ColourRgba world_line_colour = LIGHTGRAY_RGBA; //{128, 99, 42, 100};
// Game-region placement in logical screen units.
Vector2d game_region_origin, game_region_end = {0};
Vector2d world_u = {1, 0};
Vector2d world_v = {0, 1};
Vector2d game_region_resolution = {0};
float gravity = 10;
// Objects and properties
int next_object_id = 1; // Global variable to keep track of the next available ID for NewtonObjects
static ColourRgba polygonoid_line_colour = {155, 0, 0, 255};
static ColourRgba polygonoid_text_colour = {64, 64, 64, 255};
static float polygonoid_radius_default = 1.0;
static float polygonoid_mass_default = 1.0;
static Vector2d polygonoid_velocity_default = {1.0, 1.0};
static Vector2d polygonoid_acceleration_default = {0.0, 0.0f};

// Logical->pixel-space conversion properties
// static Vector2d screen_game_origin, screen_game_end = {0};
Camera2d camera_world = {0};
static float camera_world_zoom = 1.0;
static float camera_world_rotation = 0.0;
LArray *test = NULL;
//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region);
void CreateAddNewtonoid(int vertice_count, float radius, ShapeType shape_type, float mass, ColourRgba colour, Vector2d coords_center, Vector2d velocity, Vector2d acceleration);
void UpdateCameraWorldMarker(void);
void TogglePause(WorldState *context);

static World2d *GetWorldByIndexInternal(int index)
{
    return Universe_GetWorld(&G_Universe, index);
}

// FIRST: Initialisation of Gameplay Screen
void InitWorldSystem(void)
{
    // Init Global World State
    G_WorldState.selected_object = NULL;
    G_WorldState.selected_cell = NULL;
    G_WorldState.newtonoid_params = AllocateBytes(sizeof(Newtonoid2dParams));
    // Initialise command queue for UI->World commands
    extern void InitCommandQueue(void);
    InitCommandQueue();

    // 1. INIT CAMERAS using using the resolutions, sceen basis, origins etc. from Step 0
    // 1.1 Game world camera
    // Basis2d world_basis = (Basis2d){world_u, world_v};
    // Basis2d game_viewport_basis = (Basis2d){game_viewport_u, game_viewport_v};
    // camera_world = CreateCamera2d(game_viewport_basis, world_basis, game_viewport_origin, game_region_origin, camera_world_zoom, camera_world_rotation);
    // camera_world.camera_coords = (Vector2d){game_region_resolution.x * 0.5, game_region_resolution.y * 0.5};
    // UpdateCameraTransforms(&camera_world);

    // int new_world_index = Universe_CreateWorld(&G_Universe,
    //                                            world_basis,
    //                                            world_fill_colour,
    //                                            world_line_colour,
    //                                            &camera_world,
    //                                            camera_world_marker_colour,
    //                                            &G_WorldState);

    // // Select the newly created world to make it active from startup.
    // if (new_world_index >= 0)
    //     Universe_SelectWorld(&G_Universe, new_world_index, game_region_origin);

    //UpdateCameraWorldMarker();
}

void DrawGameWorld()
{
    DrawUniverse();
}

// Gameplay Screen Update logic
void UpdateWorldSystem(int mouse_x, int mouse_y)
{
    // Process any pending UI->World commands
    extern void ProcessCommandQueue(void);
    ProcessCommandQueue();

    bool cursor_in_game_viewport = mouse_x >= game_viewport_origin.x && mouse_x <= (game_viewport_origin.x + (game_viewport_u.x * game_region_resolution.x)) && mouse_y >= game_viewport_origin.y && mouse_y <= (game_viewport_origin.y + (game_viewport_v.y * game_region_resolution.y));
    UpdateWorldRegion(mouse_x, mouse_y, cursor_in_game_viewport);
}
// Gameplay Screen Stage Update logic
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    World2d *active_world = GetSelectedWorld();

    // If no world is selected, handle universe camera input and world selection
    if (!active_world)
    {
        UpdateUniverseInput(mouse_x, mouse_y, cursor_in_region);
        return;
    }

    Vector2d click_pixel_coords = {mouse_x, mouse_y};
    // Camera source origin is game_region_origin, so dest_to_source gives local + game_region_origin.
    // Subtract game_region_origin to get local coords. The universe camera ensures the selected
    // world always renders aligned to game_region_origin regardless of universe_position.
    Vector2d click_coords_raw = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords);
    Vector2d click_world_coords = VectorSum_2d(click_coords_raw, VectorScale_2d(game_region_origin, -1.0f));

    // DEFAULT TESTING SPAWN of polygonoids with random properties
    float radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.8);
    float mass = radius * polygonoid_mass_default;
    int vertice_count = 7;
    Vector2d velocity = {.x = GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), .y = GetRandomFloat(polygonoid_velocity_default.y * 8, polygonoid_velocity_default.y * -8)};
    Vector2d acceleration = polygonoid_acceleration_default;
    ColourRgba colour = polygonoid_line_colour;

    if (IsKeyPressed(KEY_SPACE))
    {
        TogglePause(&G_WorldState);
    }

    // DEBUGGING - we will update object vectors if button is pressed
    // bool keyDown = true;//IsKeyDown(KEY_LEFT_CONTROL);
    UpdateCameraWorldMarker();

    if (G_WorldState.mode != PAUSED)
    {
        // PrintCurrentBytesAlloc();
        UpdateWorld(&G_WorldState, frame_counter.delta_time);
        // PrintCurrentBytesAlloc();
    }
    // DEBUGGING - 1 Frame setp-through
    if (IsKeyPressed(KEY_LEFT_SHIFT) && G_WorldState.mode == PAUSED)
    {
        // PrintCurrentBytesAlloc();
        UpdateWorld(&G_WorldState, frame_counter.delta_time);
        // PrintCurrentBytesAlloc();
    }

    // DEBUGGING - Rapid firing of polygonoids
    // keyDown = IsKeyDown(KEY_ONE);
    // if (keyDown & frame_counter.total_frames % 3 == 0)
    // {
    //     radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.25);
    //     mass = radius * polygonoid_mass_default;
    //     velocity = (Vector2d){GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), GetRandomFloat(polygonoid_velocity_default.y * 0, polygonoid_velocity_default.y * 16)};
    //     Vector2d top_middle_world = (Vector2d){world.coord_space_grid.object.boxed_dimensions.x * 0.5, 0.3};
    //     // Vector2d top_middle_world_pixel = TransformCoordinates(camera_world.source_to_dest_mtx, top_middle_world);
    //     CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, top_middle_world, velocity, acceleration);
    //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
    // }

    // // DEBUGGING - Collisions
    // if (IsKeyPressed(KEY_THREE))
    // {
    //     radius = 1.5;
    //     mass = radius * polygonoid_mass_default;
    //     velocity = (Vector2d){-3, 3};
    //     CreateAddNewtonoid(3, radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // triangle
    //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
    // }
    // if (IsKeyPressed(KEY_FOUR))
    // {
    //     radius = 1.5;
    //     mass = radius * polygonoid_mass_default;
    //     velocity = (Vector2d){-3, 3};
    //     CreateAddNewtonoid(4, radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // rectangle
    //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
    // }
    // Handle UP click: check if clicking different world or spawning in current world
    if (IsKeyPressed(KEY_UP) && cursor_in_region)
    {
        Vector2d click_pixel_coords = {mouse_x, mouse_y};
        Vector2d click_universe_coords = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, click_pixel_coords);
        Vector2d local_coords = {0};

        // Find which world (if any) was clicked
        int clicked_world_idx = -1;
        for (int i = 0; i < G_Universe.world_count; i++)
        {
            World2d *w = &G_Universe.worlds[i];
            Vector2d local = VectorSum_2d(click_universe_coords,
                                          VectorScale_2d(w->universe_position, -1.0f));
            Vector2d res = w->coord_space_grid.coord_space.resolution_ixj;
            if (local.x >= 0 && local.y >= 0 && local.x < res.x && local.y < res.y)
            {
                clicked_world_idx = i;
                local_coords = local;
                break;
            }
        }

        if (clicked_world_idx >= 0)
        {
            // A world was clicked
            if (clicked_world_idx == G_Universe.selected_world_index)
            {
                // Click was in the currently selected world; spawn object
                CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, click_world_coords, velocity, acceleration);
                finishScreen = 1;
            }
            else
            {
                // Different world was clicked; select it
                Universe_SelectWorld(&G_Universe, clicked_world_idx, game_region_origin);
            }
        }
        else
        {
            // No world was hit; deselect and reset camera offset so all worlds are visible
            G_Universe.selected_world_index = -1;
            G_Universe.camera_offset = ZERO_VECTOR_2D;
        }
    }

    // Camera zoom in/out with Ctrl + = or Ctrl + -
    if (IsKeyDown(KEY_LEFT_CONTROL) && cursor_in_region)
    {
        if (IsKeyPressed(KEY_EQUAL))
        {
            ZoomCamera(&camera_world, 1.1f); // Zoom in
        }
        else if (IsKeyPressed(KEY_MINUS))
        {
            ZoomCamera(&camera_world, 1.0 / 1.1); // Zoom out
        }
        // Update the camera's transformation matrices after changing the zoom

        // Vector2d scaled_u = VectorScale_2d(world_u, camera_world.zoom);
        // Vector2d scaled_v = VectorScale_2d(world_v, camera_world.zoom);
        // Basis2d scaled_basis = (Basis2d){scaled_u, scaled_v};
        // camera_world.destination_basis = scaled_basis;
        // camera_world.source_to_dest_mtx = CoordSpaceTransform_2d(camera_world.source_basis, camera_world.destination_basis, camera_world.source_origin_coords);
        // camera_world.dest_to_source_mtx = MatrixInvert_3x3(camera_world.source_to_dest_mtx);
    }

    if (IsKeyDown(KEY_LEFT_SHIFT) && cursor_in_region)
    {
        if (IsKeyPressed(KEY_EQUAL))
        {
            RotateCamera(&camera_world, 0.1f); // Rotate clockwise
        }
        else if (IsKeyPressed(KEY_MINUS))
        {
            RotateCamera(&camera_world, -0.1f); // Rotate anti-clockwise
        }
        // Update the camera's transformation matrices after changing the zoom

        // Vector2d scaled_u = VectorScale_2d(world_u, camera_world.zoom);
        // Vector2d scaled_v = VectorScale_2d(world_v, camera_world.zoom);
        // Basis2d scaled_basis = (Basis2d){scaled_u, scaled_v};
        // camera_world.destination_basis = scaled_basis;
        // camera_world.source_to_dest_mtx = CoordSpaceTransform_2d(camera_world.source_basis, camera_world.destination_basis, camera_world.source_origin_coords);
        // camera_world.dest_to_source_mtx = MatrixInvert_3x3(camera_world.source_to_dest_mtx);
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }
    if (!cursor_in_region)
    {
        return;
    }

    // ----DEBUG
    char log[256] = "";
    int offset = 0;
    offset += snprintf(log + offset, sizeof(log) - offset, "WORLD (%.1f,%.1f) --> ", game_region_origin.x, game_region_origin.y);
    // DEBUG--- //

    // Check if a click is on an object and print some info about that object if so
    click_pixel_coords = (Vector2d){mouse_x, mouse_y};
    // Universe camera guarantees the selected world is always rendered at game_region_origin.
    // Subtract game_region_origin (the camera source origin) to get local coords directly.
    Vector2d click_universe_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords);
    Vector2d click_local_coords = VectorSum_2d(click_universe_coords, VectorScale_2d(game_region_origin, -1.0f));

    int cell_index = ((int)click_local_coords.y * (int)game_region_resolution.x) + (int)click_local_coords.x;
    if (click_local_coords.x < 0 || click_local_coords.y < 0 || click_local_coords.x >= game_region_resolution.x || click_local_coords.y >= game_region_resolution.y)
    {
        // Click is outside the structural world viewport boundaries! Avoid resolving cell.
        return;
    }

    // Check if there are any objects in that cell and print info about those objects if so
    Cell *cells = active_world->coord_space_grid.coord_space.cells.items;
    Cell *p_cell = &cells[cell_index];
    Cell cell = *p_cell;
    G_WorldState.selected_cell = p_cell;

    // ----DEBUG--- //
    offset += snprintf(log + offset, sizeof(log) - offset, "CELL %d(%.0f,%.0f) Occ:%d Val:%.1f --> ENTITIES ", cell_index, cell.coords_origin.x, cell.coords_origin.y, cell.occupancy, cell.value);

    // Check World objects for the object with the same ID as the one in the cell and print its properties if found
    Newtonoid2d *objs = (Newtonoid2d *)active_world->objects.items;
    Vector2d click_to_obj_vec = {active_world->coord_space_grid.coord_space.resolution_ixj.x, active_world->coord_space_grid.coord_space.resolution_ixj.y}; // Init with the max value in the world's coord system
    float shortest_dist = fabs(VectorMagnitude_2d(click_to_obj_vec));
    Newtonoid2d *p_closest = NULL;
    for (int i = 0; i < cell.occupancy; i++)
    {
        int cell_id = cell.object_ids[i];
        if (cell_id == 0)
        {
            // No object Id  at this index yet (still at initialised value).
            continue;
        }
        if (cell_id < 0)
        {
            LOG_ERROR("Object Id stored in Cell is < 0 (%.1f)\n", cell_id);
            continue;
        }

        Newtonoid2d *p = GetEntityByID(&G_WorldState, cell_id); // &objs[cell_id - 1]; // Minus 1 because the Id starts at 1, not 0;

        if (!p)
        {
            continue;
        }
        int obj_id = p->id;
        // It SHOULD equal the object ID...
        if (obj_id == cell_id)
        {
            // Check if the click was within p_closest
            Surface2d surface = p->surface;
            Vector2d vertice_offset = p->coords_center;
            bool click_in_object = IsPointInPolygon(click_local_coords, (Vector2d *)surface.surface_vectors.items, vertice_offset, surface.surface_vectors.count);

            Vector2d click_to_obj_dist_i = VectorSum_2d(VectorScale_2d(p->coords_center, -1), click_local_coords);
            float click_to_obj_mag_i = fabs(VectorMagnitude_2d(click_to_obj_dist_i));
            if (click_to_obj_mag_i < shortest_dist && click_in_object)
            {
                shortest_dist = click_to_obj_mag_i;
                p_closest = p;
            }

            // ----DEBUG--- //
            offset += snprintf(log + offset, sizeof(log) - offset, "[ID:%d POS:%.1f,%.1f] ", i + 1, obj_id, p->coords_center.x, p->coords_center.y);
        }
        else
        {
            LOG_ERROR("Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. ID in CELL = %d. ID in ENTITY = %d.\n", cell_id, obj_id);
            continue;
        }
    }

    if (p_closest)
    {
        G_WorldState.selected_object = p_closest;
        // ----DEBUG---- //
        offset += snprintf(log + offset, sizeof(log) - offset, "--> SELECTED ENTITY: ID:%d", p_closest->id);
    }
    else
    {
        G_WorldState.selected_object = NULL;
        // ----DEBUG---- //
        offset += snprintf(log + offset, sizeof(log) - offset, " --> SELECTED ENTITY: NULL");
    }
    // ----DEBUG---- //
    LOG_INFO("CLICKED (%d,%d) | %s\n", mouse_x, mouse_y, log);
}

void UpdateCameraWorldMarker(void)
{
    int idx = G_Universe.selected_world_index;
    if (idx < 0 || idx >= G_Universe.world_count)
        return;

    int camera_world_marker_id = G_Universe.camera_marker_ids[idx];
    if (camera_world_marker_id <= 0)
        return;

    Newtonoid2d *marker = GetEntityByID(&G_WorldState, camera_world_marker_id);
    if (!marker)
        return;

    marker->coords_center = camera_world.camera_coords;
    marker->coords_origin = (Vector2d){marker->coords_center.x - (marker->boxed_dimensions.x * 0.5f),
                                       marker->coords_center.y - (marker->boxed_dimensions.y * 0.5f)};
}

void CreateAddNewtonoid(int vertice_count, float radius, ShapeType shape_type, float mass, ColourRgba colour, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
    World2d *active_world = GetSelectedWorld();
    if (!active_world)
    {
        return;
    }

    Newtonoid2d new_newtonoid = {0};
    // new_newtonoid.entity_layer = FLAG_TYPE_POLYGONOID;
    // new_newtonoid.collision_mask = FLAG_TYPE_POLYGONOID | FLAG_TYPE_WALL;
    // new_newtonoid.flags = FLAG_STATUS_ALIVE | FLAG_TYPE_POLYGONOID | FLAG_ATTR_RIGID;

    switch (shape_type)
    {
    case SHAPE_MATH_EQUIDISTANT:
        new_newtonoid = CreateNewtonoid2d_Symmetric(vertice_count, radius, colour, mass, coords_center, velocity, acceleration);
        break;
    case SHAPE_MATH_POLY_HULL:
        float min_radius = GetRandomFloat(0, radius);
        new_newtonoid = CreateNewtonoid2d_Irregular(vertice_count, min_radius, radius, colour, mass, coords_center, velocity, acceleration);
        break;
    default:
        break;
    }

    if (new_newtonoid.radius > 0.0)
        AddObjectToWorld(active_world, &new_newtonoid, active_world->coord_space_grid.object.id);

    // Array_Push(polygonoids, &newPolygonoid);
}

void TogglePause(WorldState *context)
{
    if (context->mode == RUNNING)
    {
        context->mode = PAUSED;
    }
    else if (context->mode == PAUSED)
    {
        context->mode = RUNNING;
    }
}

Newtonoid2d *ResolveEntityParamsToEntity(Newtonoid2dParams *newtonoid_params)
{
    // Parameter Validation Guards
    // if (newtonoid_params->vertice_count <= 0 || newtonoid_params->vertice_count < newtonoid_params->edge_count - 1)
    // {
    //     LOG_WARN("Invalid vertice count: vertice_count = %d, edge_count = %d\n", newtonoid_params->vertice_count, newtonoid_params->edge_count);
    //     return NULL;
    // }
    if (newtonoid_params->vertice_count <= 0)
    {
        LOG_WARN("Invalid vertice_count: vertice_count = %d\n", newtonoid_params->vertice_count);
        return NULL;
    }
    if (newtonoid_params->width <= 0.0 || newtonoid_params->height <= 0.0)
    {
        LOG_WARN("Invalid size: width = %f, height = %f\n", newtonoid_params->width, newtonoid_params->height);
        return NULL;
    }

    // Resolve the Shape Type
    // If it has 4 vertices and 4 edges, it's a box. If it has high vertex counts, it approximates a circle/equidistant shape.
    ShapeType shape_type = SHAPE_MATH_EQUIDISTANT;
    Newtonoid2d *obj = CreateNewtonoid2d_Reference(newtonoid_params->mass, newtonoid_params->coords_center, newtonoid_params->velocity, newtonoid_params->acceleration, (Surface2d){0});
    if (!obj)
    {
        LOG_WARN("Failed to allocate new physical object. World entity pool full.\n");
        return NULL;
    }
    obj->surface.surface_vectors = MakeLArray(newtonoid_params->vertice_count, sizeof(Vector2d));
    if (newtonoid_params->vertice_count == 4)
    {
        obj->shape_type = SHAPE_BOX;
        CalcBoxVertices((Vector2d){newtonoid_params->width, newtonoid_params->height}, ZERO_VECTOR_2D, (Vector2d *)obj->surface.surface_vectors.items);
    }
    else if (newtonoid_params->vertice_count == 0)
    {
        obj->shape_type = SHAPE_CIRCLE;
    }
    else
    {
        obj->shape_type = shape_type;
    }

    if (shape_type == SHAPE_MATH_EQUIDISTANT)
    {
        // Generate regular polygon vertices distributed equidistantly around a unit radius loop
        int total_verts = newtonoid_params->vertice_count;
        float radius_x = newtonoid_params->width * 0.5f;
        float radius_y = newtonoid_params->height * 0.5f;
        obj->surface.surface_vectors = CreateVertices_Symmetric(total_verts, radius_x, radius_y);
    }

    LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
             obj->id, shape_type, obj->coords_center.x, obj->coords_center.y);

    return obj;
}
// void CreateAddPolygonoid(int vertice_count, float radius, ShapeType shape_type, float mass, ColourRgba colour, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
// {
//     Polygonoid new_polygonoid = {0};
//     new_polygonoid.newtonian_properties.entity_layer = FLAG_POLYGONOID;
//     new_polygonoid.newtonian_properties.collision_mask = FLAG_POLYGONOID | FLAG_WALL;
//     new_polygonoid.newtonian_properties.status_flags = FLAG_ALIVE;

//     switch (shape_type)
//     {
//     case SHAPE_MATH_EQUIDISTANT:
//         new_polygonoid = CreatePolygonoid_Symmetric(vertice_count, radius, colour, mass, coords_center, velocity, acceleration);
//         break;
//     case SHAPE_MATH_POLY_HULL:
//         float min_radius = GetRandomFloat(0, radius);
//         new_polygonoid = CreatePolygonoid_Irregular(vertice_count, min_radius, radius, colour, mass, coords_center, velocity, acceleration);
//         break;
//     default:
//         break;
//     }

//     if (new_polygonoid.radius > 0.0)
//         AddObjectToWorld(&world, &new_polygonoid, world.coord_space_grid.object.id);

//     // Array_Push(polygonoids, &newPolygonoid);
// }

int GetNewtonoidCount(void)
{
    World2d *active_world = GetSelectedWorld();
    return active_world ? active_world->objects.count : 0;
}

int CreateNewWorldDefault(void)
{
    Basis2d world_basis = (Basis2d){world_u, world_v};
    int new_index = Universe_CreateWorld(&G_Universe,
                                         world_basis,
                                         world_fill_colour,
                                         world_line_colour,
                                         &camera_world,
                                         camera_marker_colour,
                                         &G_WorldState);
    if (new_index >= 0)
    {
        World2d *w = Universe_GetWorld(&G_Universe, new_index);
        if (w)
        {
            Universe_SetWorldBounds(&G_Universe,
                                    new_index,
                                    w->universe_position,
                                    VectorSum_2d(w->universe_position, w->coord_space_grid.coord_space.resolution_ixj));
        }
    }
    return new_index;
}

bool SelectWorldByIndex(int index)
{
    bool ok = Universe_SelectWorld(&G_Universe, index, game_region_origin);
    if (ok)
    {
        World2d *w = Universe_GetSelectedWorld(&G_Universe);
        G_WorldState.world = w;
        G_WorldState.entity_world_index_registry = w ? &w->entity_world_index_registry : NULL;
        G_WorldState.collisions = w ? &w->collisions : NULL;
        G_WorldState.selected_object = NULL;
        G_WorldState.selected_cell = NULL;
    }
    return ok;
}

int GetWorldCount(void) { return Universe_GetWorldCount(&G_Universe); }
int GetSelectedWorldIndex(void) { return Universe_GetSelectedIndex(&G_Universe); }
World2d *GetSelectedWorld(void) { return Universe_GetSelectedWorld(&G_Universe); }
World2d *GetWorldByIndex(int i) { return Universe_GetWorld(&G_Universe, i); }

Vector2d *GetNextWorldSpawnOriginPtr(void) { return &G_Universe.next_spawn; }
void SetNextWorldSpawnOrigin(Vector2d v) { G_Universe.next_spawn = v; }
Vector2d *GetNextWorldResolutionPtr(void) { return &G_Universe.next_resolution; }
float *GetNextWorldGravityPtr(void) { return &G_Universe.next_gravity; }
Vector2d GetUniverseCameraOffset(void) { return G_Universe.camera_offset; }

// Gameplay Screen Unload logic
void UnloadGameplayScreen(void)
{
    ShutdownJobSystem();
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void)
{
    return finishScreen;
}

// void UpdatePolygonoidVectors(DArray *polygonoids)
// {
//     // Update Polygonoids
//     if (polygonoids == NULL || polygonoids->count <= 0)
//     {
//         return; // Nothing to update
//     }
//     Polygonoid *p = Enumerate(polygonoids);
//     if (p == NULL)
//     {
//         fprintf(stderr, "Failed to retrieve enumerated Polygonoid\n"); // Enumerator failed to retrieve the first item
//     }
//     while (p != NULL)
//     {
//         if (&p->newtonian_properties != NULL)
//         {
//             CalculateVectors(&p->newtonian_properties, frame_counter.delta_time);
//         }
//         p = Enumerate(polygonoids);
//     }
//     ResetEnumerator(polygonoids); // Reset enumerator after drawing
// }

// Gameplay Screen should finish
// Vector2d WorldToScreenCoordinates(Matrix3x3 basis_transform, Vector2d world_coordinates)
// {
//     Vector2d screen_coords;

//     // 1. Get the "transformation" or "mapping" basis to go from world to screen.
//     // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

//     // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
//     //  Multiply: (Row 1 * WorldColumn)
//     //  screenX = (m0 * x) + (m3 * y) + m6
//     screen_coords.x = (world_coordinates.x * basis_transform.m0) + (world_coordinates.y * basis_transform.m3) + basis_transform.m6;

//     // Multiply: (Row 2 * WorldColumn)
//     // screenY = (m1 * x) + (m4 * y) + m7
//     screen_coords.y = (world_coordinates.x * basis_transform.m1) + (world_coordinates.y * basis_transform.m4) + basis_transform.m7;

//     return screen_coords;
// }

// Vector2d WorldToScreenBasisTransformVector(Basis2d world_basis, Basis2d screen_basis, Vector2d screen_origin_coordinates, Vector2d world_coordinates)
// {
//     Vector2d screen_coordinates;

//     // Need to translate the world's basis to screen basis. Therefore need basis_u and basis_v scaling factor
//     float world_basis_u_mag = VectorMagnitude_2d(world_basis.u);
//     float world_basis_v_mag = VectorMagnitude_2d(world_basis.v);

//     float screen_basis_u_mag = VectorMagnitude_2d(screen_basis.u);
//     float screen_basis_v_mag = VectorMagnitude_2d(screen_basis.v);

//     float scale_u = screen_basis_u_mag / world_basis_u_mag;
//     float scale_v = screen_basis_v_mag / world_basis_v_mag;

//     // Scale the world coordinates by the basis scaling factors to get the coordinates in terms of the screen basis
//     screen_coordinates.x = (world_coordinates.x * scale_u) + screen_origin_coordinates.x;
//     screen_coordinates.y = (world_coordinates.y * scale_v) + screen_origin_coordinates.y;
//     return screen_coordinates;
// }

// Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
// {
//    Vector2d origin = field.shape.object.position;
//    Vector2d u = field.coordinateSpace.basis.u;
//    Vector2d v = field.coordinateSpace.basis.v;

//    // // Get position relative to the grid origin
//    // float px = objectPos.x - origin.x;
//    // float py = objectPos.y - origin.y;

//    // // Calculate the Determinant
//    // float det = (u.x * v.y) - (u.y * v.x);

//    // // If determinant is 0, the grid is collapsed (invalid)
//    // if (fabs(det) < 0.0001f)
//    //    return (Vector2d){-1, -1};

//    // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
//    // float c = (px * v.y - py * v.x) / det;
//    // float r = (py * u.x - px * u.y) / det;

//    // // Use floor() to get the integer index of the cell
//    // return (Vector2d){floorf(c), floorf(r)};
// }
// // Gameplay Screen Draw logic
// void DrawGameplayScreen(void)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
//     Vector2 pos = { 20, 100 };
//     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
//     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// }
// Gameplay Screen - Main stage Draw
// void DrawGameplayScreenStage(int startX, int startY, int width, int height, Color color)
// {
//     // Stage canvas for circloids to interact on
//     DrawRectangle(startX, startY, width, height, color);

//     // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
//     // DrawWorld(world);
//     // DrawWorldCoordinateSpace(world.world_space);
//     //  Draw fields here
//     //  DrawFields_Rect();

//     // Draw circloids last so that they are on top of the fields
//     // DrawCircloids();
// }

// void DrawCircloids(void)
// {
//     if (circloids == NULL) // || circloids->coll == NULL || circloids->coll->count <= 0)
//     {
//         return; // No circloids to draw
//     }
//     Collection *circloid_coll = &circloids->coll;
//     for (int i = 0; i < circloid_coll->count; i++)
//     {
//         Circloid *circloid = (Circloid *)((char *)circloid_coll->items + (i * circloid_coll->elemSize));
//         Vector2d circloidPos = circloid->newtonian_properties.world_position;
//         Vector2d cellIndices = GetCellIndicesFromCoordinates(position_field.shape.newtonian_properties.world_position, circloid->newtonian_properties.world_position, position_field.coordinateSpace.basis);

//         // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

//         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)", ((int)cellIndices.x + 1) * ((int)cellIndices.y + 1), (int)cellIndices.x + 1, (int)cellIndices.y + 1, (int)circloidPos.x, (int)circloidPos.y);
//         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

//         // Draw circloid THEN text so text is on top
//         DrawCircle(circloidPos.x, circloidPos.y, circloid->radius, (Color)DARKBROWN_RGBA);
//         DrawTextEx(font, displayText, (Vector2){circloidPos.x - 0.7 * circloid->radius, circloidPos.y - 0.7 * circloid->radius}, font.baseSize, 1, (Color)BEIGE_RGBA);

//         // Debug print
//         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
//     }
//     // Circloid *circloid = Enumerate(circloids->coll);
//     // if (circloid == NULL)
//     // {
//     //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
//     // }
//     // while (circloid != NULL)
//     // {
//     //     Vector2d pos = circloid->object.pos;
//     //     Vector2d cell = GetCellFromCoordinates(position_field, circloid->object.pos);
//     //     DrawCircle(pos.x, pos.y, circloid->radius, (Color)DARKBROWN_RGBA);

//     //     // Output the circloid's position and cell as text on top of it
//     //     const char *cellText = TextFormat("Cell: (%d,%d)", (int)cell.x, (int)cell.y);
//     //     const char *posText = TextFormat("Coord: (%d,%d)", (int)pos.x, (int)pos.y);
//     //     const char *allText = TextFormat("%s\n%s", cellText, posText);
//     //     DrawTextEx(font, allText, (Vector2){pos.x, pos.y}, font.baseSize, 1, (Color)BEIGE_RGBA);
//     //     // DrawTextEx(font, cellText, (Vector2){pos.x - (circloid->radius / 2), pos.y - (circloid->radius / 2)}, font.baseSize, 1, (Color)DARKGREEN_RGBA);

//     //     circloid = Enumerate(circloids->coll);
//     // }
//     // ResetEnumerator(circloids->coll); // Reset enumerator after drawing
// }

// void DrawFields_Rect(void)
// {
//     if (position_field.coordinateSpace.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
//     {
//         return; // No field to draw
//     }
//     // Draw background
//     DrawRectangle(position_field.shape.newtonian_properties.world_position.x, position_field.shape.newtonian_properties.world_position.y, position_field.shape.width, position_field.shape.height, (Color){world_bg_colour.r, world_bg_colour.g, world_bg_colour.b, world_bg_colour.a});

//     int rows = position_field.coordinateSpace.rows;
//     int cols = position_field.coordinateSpace.columns;
//     int totalUnits = rows * cols;
//     CoordinateSpace coordinateSpace = position_field.coordinateSpace;
//     ColourRgba colour = position_field.lineColour;
//     Color color = (Color){colour.r, colour.g, colour.b, colour.a};

//     // Draw "Horizontal-ish" lines (Rows)
//     // These lines start at (origin + r*v) and end at (origin + r*v + cols*u)
//     Vector2d origin = position_field.shape.newtonian_properties.world_position;
//     for (int r = 0; r < coordinateSpace.lineSegments_u.coll.count; r++)
//     {
//         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_u.coll.items + (r * coordinateSpace.lineSegments_u.coll.elemSize));
//         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
//     }

//     // Draw "Vertical-ish" lines (Columns)
//     // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
//     for (int c = 0; c < coordinateSpace.lineSegments_v.coll.count; c++)
//     {
//         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_v.coll.items + (c * coordinateSpace.lineSegments_v.coll.elemSize));
//         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
//     }

//     // Draw field unit values as text on top of each field unit
//     Collection *cells = &position_field.coordinateSpace.cells.coll;
//     // int textOffsetX = (position_field.coordinateSpace.basis.u.x + position_field.coordinateSpace.basis.v.x) / 2;
//     // int textOffsetY = (position_field.coordinateSpace.basis.u.y + position_field.coordinateSpace.basis.v.y) / 2;
//     for (int i = 0; i < totalUnits; i++)
//     {
//         int row = i / cols;
//         int col = i % cols;
//         Cell *cell = (Cell *)((char *)cells->items + (i * cells->elemSize));
//         Vector2d cellPos = cell->coords;
//         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)\nValue: %.1f", i + 1, row + 1, col + 1, (int)cellPos.x, (int)cellPos.y, cell->value);
//         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
//         DrawTextEx(font, displayText, (Vector2){cellPos.x, cellPos.y}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

//         // Debug print
//         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
//     }
//     // printf("Drew %d cells\n", count);
//     //  Use the number of rows, columns, and their dimensions to draw field lines as rectangles
//     //  Method 1: Enumerate the grid
//     //  float *cell = Enumerate(position_field.cells->coll);
//     //  if (cell == NULL)
//     //  {
//     //      fprintf(stderr, "Failed to retrieve enumerated field unit\n"); // Enumerator failed to retrieve the first item
//     //  }
// }