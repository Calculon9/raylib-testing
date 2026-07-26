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
#include <stdarg.h>
#include "raylib.h"
#include "system/utility_system.h"
#include "system/world_system.h"
#include "system/universe_system.h"
#include "system/viewport_system.h"
#include "system/systems.h"
#include "system/job_system.h"
#include "physics/physics.h"
#include "common/common.h"
#include "world/world.h"
#include "world/universe.h"
#include "camera/camera.h"
#include "math/affine_space_ops.h"
#include <math/helpers.h>
#include "system/drag_interaction.h"
// #include "screens.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
// ----------WORLD SCREEN----------
static int finishScreen = 0;
bool world_grid_debug_labels_enabled = true;
// Game-region placement in logical screen units.
// Vector2d game_viewport_local_origin, game_viewport_local_end = {0};
// Vector2d game_viewport_local_resolution = {0};
float gravity = 10;
// Objects and properties
int next_object_id = 1; // Global variable to keep track of the next available ID for NewtonObjects
static ColourRgba polygonoid_line_colour = {155, 0, 0, 255};
static float polygonoid_radius_default = 1.0;
static float polygonoid_mass_default = 1.0;
static Vector2d polygonoid_velocity_default = {1.0, 1.0};
static Vector2d polygonoid_acceleration_default = {0.0, 0.0f};

typedef struct
{
    Newtonoid2d *entity;
    Vector2d velocity;
    Vector2d acceleration;
    Vector2d momentum;
    bool has_snapshot;
} EntityDragMotionSnapshot;

static EntityDragMotionSnapshot g_drag_motion_snapshot = {0};
//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region);
void CreateAddNewtonoid(int vertice_count, float radius, ShapeType shape_type,
                        float mass, ColourRgba colour, Vector2d coords_center,
                        Vector2d velocity, Vector2d acceleration);
void TogglePause(World2d *world);
static Newtonoid2d *ResolveClosestEntityAt(World2d *active_world, Vector2d click_local_coords,
                                           Cell **out_cell, int *out_cell_index,
                                           char *log, size_t log_size, int *log_offset);

static void SnapshotDraggedEntityMotion(Newtonoid2d *entity)
{
    if (!entity)
    {
        g_drag_motion_snapshot.entity = NULL;
        g_drag_motion_snapshot.has_snapshot = false;
        return;
    }

    g_drag_motion_snapshot.entity = entity;
    g_drag_motion_snapshot.velocity = entity->velocity;
    g_drag_motion_snapshot.acceleration = entity->acceleration;
    g_drag_motion_snapshot.momentum = entity->momentum;
    g_drag_motion_snapshot.has_snapshot = true;
}

static void RestoreDraggedEntityMotion(Newtonoid2d *entity)
{
    if (!entity || !g_drag_motion_snapshot.has_snapshot)
    {
        return;
    }

    if (g_drag_motion_snapshot.entity != entity)
    {
        return;
    }

    entity->velocity = g_drag_motion_snapshot.velocity;
    entity->acceleration = g_drag_motion_snapshot.acceleration;
    entity->momentum = g_drag_motion_snapshot.momentum;

    g_drag_motion_snapshot.entity = NULL;
    g_drag_motion_snapshot.has_snapshot = false;
}

static void AppendLogLine(char *buffer, size_t buffer_size, int *offset,
                          const char *fmt, ...)
{
    if (!buffer || !offset || !fmt)
    {
        return;
    }

    if (*offset < 0 || (size_t)*offset >= buffer_size)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer + *offset, buffer_size - (size_t)*offset, fmt, args);
    va_end(args);

    if (written > 0)
    {
        *offset += written;
    }
}

static Vector2d ResolvePixelToWorldFrame(const World2d *active_world, Vector2d pixel_coords)
{
    if (!active_world)
    {
        return ZERO_VECTOR_2D;
    }
    // Convert click pixel coordinates from pixel --> viewport --> universe --> world-local coordinates
    // Frame2d input_frame = screen_frame;
    // Frame2d parent_frame = *G_Universe.camera.tunnel.source_frame;// ? *G_Universe.camera.tunnel.source_frame : CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, G_Universe.resolution);
    // Frame2d child_frame = *active_world->camera.tunnel.source_frame;// ? *active_world->camera.tunnel.source_frame : active_world->grid_space.space.frame;

    Matrix3x3 game_viewport_inv_transform = game_viewport.tunnel.dest_to_source_mtx; // pixel to viewport
    Matrix3x3 parent_inv_transform = G_Universe.camera.tunnel.dest_to_source_mtx;    // viewport to universe
    Matrix3x3 child_inv_transform = active_world->tunnel.dest_to_source_mtx;         // universe to world-local

    // Frame2d parent_system = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, active_world->grid_space.space.frame.local_max);
    // Frame2d child_local_system = active_world->grid_space.space.frame;
    // Frame2d input_system = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, active_world->grid_space.space.frame.local_max);
    // Matrix3x3 input_to_child_local_mtx = CalcChainTransform_FromFrame(input_system, parent_system, child_local_system);
    Vector2d viewport_coords = TransformCoordinates(game_viewport_inv_transform, pixel_coords);
    Vector2d parent_coords = TransformCoordinates(parent_inv_transform, viewport_coords);
    Vector2d child_coords = TransformCoordinates(child_inv_transform, parent_coords);

    return child_coords;
    LOG_INFO("ResolvePixelToWorldFrame: pixel_coords=(%.2f, %.2f) -> viewport_coords=(%.2f, %.2f) -> parent_coords=(%.2f, %.2f) -> child_coords=(%.2f, %.2f)\n",
             pixel_coords.x, pixel_coords.y,
             viewport_coords.x, viewport_coords.y,
             parent_coords.x, parent_coords.y,
             child_coords.x, child_coords.y);
    // return TransformCoordinates(input_to_child_local_mtx, click_parent_coords);
}

static bool TryGetClickedSpaceCell(Space2d *space, Vector2d click_local_coords,
                                   int *out_cell_index, Cell **out_cell)
{
    if (!space || !out_cell_index || !out_cell)
    {
        return false;
    }

    if (click_local_coords.x < 0 || click_local_coords.y < 0 || click_local_coords.x >= space->columns || click_local_coords.y >= space->rows)
    {
        return false;
    }

    int cell_index = ((int)click_local_coords.y * space->columns) + (int)click_local_coords.x;
    Cell *cells = space->cells.items;
    if (!cells)
    {
        return false;
    }

    *out_cell_index = cell_index;
    *out_cell = &cells[cell_index];
    return true;
}

static Newtonoid2d *FindClosestObjectInCell(World2d *world, const Cell *cell, Vector2d click_local_coords, Vector2d max_distance, char *log,
                                            size_t log_size, int *log_offset)
{
    if (!cell)
    {
        return NULL;
    }

    float shortest_dist = fabs(VectorMagnitude_2d(max_distance));
    Newtonoid2d *closest = NULL;

    for (int i = 0; i < cell->occupancy; i++)
    {
        int cell_id = cell->object_ids[i];
        if (cell_id == 0)
        {
            continue;
        }

        if (cell_id < 0)
        {
            LOG_ERROR("Object Id stored in Cell is < 0 (%d)\n", cell_id);
            continue;
        }

        Newtonoid2d *obj = GetEntityByID(world, cell_id);
        if (!obj)
        {
            continue;
        }

        int obj_id = obj->id;
        if (obj_id != cell_id)
        {
            LOG_ERROR("Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. ID in CELL = %d. ID in ENTITY = %d.\n", cell_id, obj_id);
            continue;
        }

        Surface2d surface = obj->surface;
        Vector2d vertice_offset = obj->coords_center;
        bool click_in_object = IsPointInPolygon(click_local_coords,
                                                (Vector2d *)surface.surface_vectors.items,
                                                vertice_offset,
                                                surface.surface_vectors.count);

        Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(obj->coords_center, -1), click_local_coords);
        float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
        if (click_to_obj_mag < shortest_dist && click_in_object)
        {
            shortest_dist = click_to_obj_mag;
            closest = obj;
        }

        AppendLogLine(log, log_size, log_offset, "[ID:%d POS:%.1f,%.1f] ", i + 1, obj_id, obj->coords_center.x, obj->coords_center.y);
    }

    return closest;
}

static Newtonoid2d *ResolveClosestEntityAt(World2d *active_world, Vector2d click_local_coords,
                                           Cell **out_cell, int *out_cell_index,
                                           char *log, size_t log_size, int *log_offset)
{
    if (!active_world)
    {
        return NULL;
    }

    Cell *cell_ptr = NULL;
    int cell_index = -1;
    if (!TryGetClickedSpaceCell(&active_world->grid_space.space, click_local_coords, &cell_index, &cell_ptr))
    {
        return NULL;
    }

    if (out_cell)
    {
        *out_cell = cell_ptr;
    }

    if (out_cell_index)
    {
        *out_cell_index = cell_index;
    }

    Cell cell = *cell_ptr;
    if (log && log_offset)
    {
        AppendLogLine(log, log_size, log_offset,
                      "CELL %d(%.0f,%.0f) Occ:%d Val:%.1f --> ENTITIES ",
                      cell_index, cell.local_origin.x, cell.local_origin.y,
                      cell.occupancy, cell.value);
    }

    Vector2d max_click_distance = {(float)active_world->grid_space.space.columns,
                                   (float)active_world->grid_space.space.rows};
    return FindClosestObjectInCell(active_world, &cell, click_local_coords,
                                   max_click_distance, log,
                                   log_size, log_offset);
}

// FIRST: Initialisation of Gameplay Screen
void InitWorldSystem(void)
{
    // Init Global World State
    G_UIState.selected_object = NULL;
    G_UIState.selected_cell = NULL;
    G_UIState.selected_cell_index = -1;
    G_UIState.newtonoid_params = AllocateBytes(sizeof(Newtonoid2dParams));
    // Initialise command queue for UI->World commands
    extern void InitCommandQueue(void);
    InitCommandQueue();
}

// Gameplay Screen Update logic
void UpdateWorldSystem(int mouse_x, int mouse_y)
{
    // Process any pending UI->World commands
    ProcessCommandQueue();

    bool cursor_in_game_viewport = mouse_x >= game_viewport.pixel_origin.x && mouse_x <= (game_viewport.pixel_origin.x + (game_viewport.pixel_u.x * game_viewport.resolution.x)) && mouse_y >= game_viewport.pixel_origin.y && mouse_y <= (game_viewport.pixel_origin.y + (game_viewport.pixel_v.y * game_viewport.resolution.y));
    if (G_Universe.world_count == 0)
        return;

    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    Vector2d click_pixel_coords = {mouse_x, mouse_y};
    Vector2d click_world_coords = ResolvePixelToWorldFrame(active_world, click_pixel_coords);
    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);

    // DEFAULT TESTING SPAWN of polygonoids with random properties
    float radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.8);
    float mass = radius * polygonoid_mass_default;
    int vertice_count = 7;
    Vector2d velocity = {.x = GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), .y = GetRandomFloat(polygonoid_velocity_default.y * 8, polygonoid_velocity_default.y * -8)};
    Vector2d acceleration = polygonoid_acceleration_default;
    ColourRgba colour = polygonoid_line_colour;

    if (active_world)
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            TogglePause(active_world);
        }

        if (active_world->mode != PAUSED)
        {
            // PrintCurrentBytesAlloc();
            UpdateWorld(active_world, frame_counter.delta_time);
            // PrintCurrentBytesAlloc();
        }
        // DEBUGGING - 1 Frame setp-through
        if (IsKeyPressed(KEY_LEFT_SHIFT) && active_world->mode == PAUSED)
        {
            // PrintCurrentBytesAlloc();
            UpdateWorld(active_world, frame_counter.delta_time);
            // PrintCurrentBytesAlloc();
        }

        if (IsKeyPressed(KEY_F7))
        {
            world_grid_debug_labels_enabled = !world_grid_debug_labels_enabled;
            printf("[World] Grid debug labels: %s\n", world_grid_debug_labels_enabled ? "ON" : "OFF");
        }

        if (IsKeyPressed(KEY_ONE))
        {
            CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, click_world_coords, velocity, acceleration);
            UpdateWorld(active_world, frame_counter.delta_time);
        }

        // DEBUGGING - Rapid firing of polygonoids
        // keyDown = IsKeyDown(KEY_ONE);
        // if (keyDown & frame_counter.total_frames % 3 == 0)
        // {
        //     radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.25);
        //     mass = radius * polygonoid_mass_default;
        //     velocity = (Vector2d){GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), GetRandomFloat(polygonoid_velocity_default.y * 0, polygonoid_velocity_default.y * 16)};
        //     Vector2d top_middle_world = (Vector2d){world.grid_space.object.boxed_dimensions.x * 0.5, 0.3};
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
        // if (IsKeyPressed(KEY_UP) && cursor_in_region)
        // {
        //     Vector2d click_pixel_coords = {mouse_x, mouse_y};
        //     Vector2d click_universe_coords = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, click_pixel_coords);

        //     // Find which world (if any) was clicked
        //     int clicked_world_idx = -1;
        //     for (int i = 0; i < G_Universe.world_count; i++)
        //     {
        //         World2d *w = &G_Universe.worlds[i];
        //         Vector2d res = w->grid_space.space.resolution_ixj;

        //         // Un-map Universe Space to enter this specific World's Local Space.
        //         // This single matrix multiplication handles the world's custom rotation, scaling, and position.
        //         Vector2d local_coords = TransformCoordinates(w->camera.source_to_dest_mtx, click_universe_coords);

        //         // Re-anchor from center pivot to Top-Left for the boundary verification.
        //         // If w->world_to_uni_inverse treats the center of the world as (0,0):
        //         float half_width = res.x * 0.5f;
        //         float half_height = res.y * 0.5f;

        //         Vector2d local_grid_pos = {
        //             local_coords.x + half_width,
        //             local_coords.y + half_height};

        //         // Because the inverse matrix completely straightened out the rotation,
        //         // we can now safely use a standard flat bounds check!
        //         if (local_grid_pos.x >= 0.0f && local_grid_pos.y >= 0.0f &&
        //             local_grid_pos.x < res.x && local_grid_pos.y < res.y)
        //         {
        //             clicked_world_idx = i;
        //             break; // Found it!
        //         }
        //     }

        //     if (clicked_world_idx >= 0)
        //     {
        //         // A world was clicked
        //         if (clicked_world_idx == G_Universe.selected_world_index)
        //         {
        //             // Click was in the currently selected world; spawn object
        //             CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, click_world_coords, velocity, acceleration);
        //             finishScreen = 1;
        //         }
        //         else
        //         {
        //             // Different world was clicked; select it
        //             Universe_SelectWorld(&G_Universe, clicked_world_idx, game_viewport_local_origin);
        //         }
        //     }
        //     else
        //     {
        //         // No world was hit; deselect and reset camera offset so all worlds are visible
        //         G_Universe.selected_world_index = -1;
        //         //G_Universe.camera_offset = ZERO_VECTOR_2D;
        //     }
        // }

        if (cursor_in_game_viewport && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            DragInteraction_UpdateButtonDown(game_drag_ctx, click_pixel_coords);

            if (game_drag_ctx->pointer_state.left_button_hold_ticks == 1)
            {
                char log[256] = "";
                int offset = 0;
                AppendLogLine(log, sizeof(log), &offset, "WORLD (%.1f,%.1f) --> ", game_viewport.local_origin.x, game_viewport.local_origin.y);

                Cell *selected_cell = NULL;
                int selected_cell_index = -1;
                Newtonoid2d *p_closest = ResolveClosestEntityAt(active_world, click_world_coords, &selected_cell, &selected_cell_index,
                                                                log, sizeof(log), &offset);

                G_UIState.selected_cell = selected_cell;
                G_UIState.selected_cell_index = selected_cell_index;

                if (p_closest)
                {
                    G_UIState.selected_object = p_closest;
                    SnapshotDraggedEntityMotion(p_closest);
                    DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_ENTITY, p_closest, p_closest->coords_center);
                    AppendLogLine(log, sizeof(log), &offset, "--> SELECTED ENTITY: ID:%d", p_closest->id);
                }
                else
                {
                    G_UIState.selected_object = NULL;
                    g_drag_motion_snapshot.entity = NULL;
                    g_drag_motion_snapshot.has_snapshot = false;
                    DragInteraction_ClearCapture(game_drag_ctx);
                    AppendLogLine(log, sizeof(log), &offset, " --> SELECTED ENTITY: NULL");
                }

                LOG_INFO("CLICKED (%d,%d) | %s\n", mouse_x, mouse_y, log);
            }

            if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY &&
                DragInteraction_IsDragActive(game_drag_ctx, 5.0f))
            {
                Newtonoid2d *dragged = (Newtonoid2d *)game_drag_ctx->target;
                if (dragged)
                {
                    Vector2d initial_world_coords = ResolvePixelToWorldFrame(active_world, game_drag_ctx->pointer_state.initial_pos);
                    Vector2d current_world_coords = ResolvePixelToWorldFrame(active_world, game_drag_ctx->pointer_state.current_pos);
                    Vector2d drag_delta_world = VectorSum_2d(current_world_coords, (Vector2d){-initial_world_coords.x, -initial_world_coords.y});

                    Vector2d new_center = VectorSum_2d(game_drag_ctx->target_anchor, drag_delta_world);
                    float max_x = fmaxf(0.0f, (float)active_world->grid_space.space.columns - 0.001f);
                    float max_y = fmaxf(0.0f, (float)active_world->grid_space.space.rows - 0.001f);
                    new_center.x = fmaxf(0.0f, fminf(new_center.x, max_x));
                    new_center.y = fmaxf(0.0f, fminf(new_center.y, max_y));

                    dragged->coords_center = new_center;
                    dragged->coords_origin = (Vector2d){
                        new_center.x - (dragged->boxed_dimensions.x * 0.5f),
                        new_center.y - (dragged->boxed_dimensions.y * 0.5f)};

                    // Keep drag interaction deterministic while the cursor is controlling the entity.
                    dragged->velocity = ZERO_VECTOR_2D;
                    dragged->acceleration = ZERO_VECTOR_2D;
                    dragged->momentum = ZERO_VECTOR_2D;
                }
            }
        }
        else if (game_drag_ctx->pointer_state.left_button_hold_ticks > 0)
        {
            if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY)
            {
                RestoreDraggedEntityMotion((Newtonoid2d *)game_drag_ctx->target);
            }
            DragInteraction_UpdateButtonUp(game_drag_ctx);
        }
    }
}

// Gameplay Screen Stage Update logic
// void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region)

void CreateAddNewtonoid(int vertice_count, float radius, ShapeType shape_type,
                        float mass, ColourRgba colour, Vector2d coords_center,
                        Vector2d velocity, Vector2d acceleration)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    if (!active_world)
    {
        return;
    }

    Newtonoid2d new_newtonoid = {0};

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
        AddObjectToWorld(active_world, &new_newtonoid, active_world->grid_space.object.id);
}

void TogglePause(World2d *world)
{
    if (world->mode == RUNNING || world->mode == PAUSED)
    {
        world->mode = (world->mode == RUNNING) ? PAUSED : RUNNING;
    }
}

Newtonoid2d *ResolveEntityParamsToEntity(Newtonoid2dParams *newtonoid_params)
{
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
int GetNewtonoidCount(void)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    return active_world ? active_world->objects.count : 0;
}

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
