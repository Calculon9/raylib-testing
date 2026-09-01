/**********************************************************************************************
 *
 * WORLD GAMEPLAY SYSTEM
 *
 * Gameplay input, world selection, entity picking, and screen-level world helpers.
 * Simulation and entity storage remain in the world module under src/world.
 *
 **********************************************************************************************/

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "raylib.h"
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "physics/physics.h"
#include "system/command_queue.h"
#include "system/debug_overlay_system.h"
#include "system/job_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "input/drag_interaction.h"
#include "combat/projectile.h"

// World-level defaults used by the gameplay screen and debug spawning controls.
bool world_grid_debug_labels_enabled = false;
float gravity = 10;
static ColourRgba polygonoid_line_colour = {194, 105, 83, 255};
static float polygonoid_radius_default = 1.0f;
static float polygonoid_mass_default = 1.0f;
static Vector2d polygonoid_velocity_default = {1.0f, 1.0f};
static Vector2d polygonoid_acceleration_default = {0.0f, 0.0f};

typedef struct
{
    Newtonoid2d *entity;
    Vector2d velocity;
    Vector2d acceleration;
    Vector2d momentum;
    uint32_t collision_mask;
    bool has_snapshot;
} EntityDragMotionSnapshot;

static EntityDragMotionSnapshot g_drag_motion_snapshot = {0};

// Forward declarations keep the input router independent of helper ordering.
void CreateAddNewtonoid(int vertice_count, float radius, ShapeBuildType build_type,
                        float mass, ColourRgba colour, Vector2d anchor_position,
                        Vector2d velocity, Vector2d acceleration);
void TogglePause(World2d *world);

// Save motion state while an entity is being dragged so manual positioning does
// not permanently erase the simulation state that existed before the drag.
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
    g_drag_motion_snapshot.collision_mask = entity->collision_mask;
    entity->collision_mask = 0;
    g_drag_motion_snapshot.has_snapshot = true;
}

// Restore the saved simulation state when an entity drag ends.
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
    entity->collision_mask = g_drag_motion_snapshot.collision_mask;

    g_drag_motion_snapshot.entity = NULL;
    g_drag_motion_snapshot.has_snapshot = false;
}

// Append diagnostic text without writing beyond the caller's fixed buffer.
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

// Convert a screen pixel into the selected world's local coordinates.
Vector2d ResolvePixelToWorldFrame(const World2d *active_world, Vector2d pixel_coords)
{
    if (!active_world)
    {
        return ZERO_VECTOR_2D;
    }

    return TransformCoordinates(ResolvePixelToWorldMatrix(active_world, &G_Universe.camera), pixel_coords);
}

// Resolve a local point to its containing grid cell before inspecting entities.
static bool TryGetClickedSpaceCell(Space2d *space, Vector2d click_local_coords, int *out_cell_index, Cell **out_cell)
{
    if (!space || !out_cell_index || !out_cell)
    {
        return false;
    }

    int cell_index = GetIndexFromCoords(space, click_local_coords);
    if (cell_index < 0)
    {
        return false;
    }

    Cell *cells = space->cells.items;
    if (!cells)
    {
        return false;
    }

    *out_cell_index = cell_index;
    *out_cell = &cells[cell_index];
    return true;
}

// Select the nearest entity whose polygon contains the click point. The cell is
// only a broad-phase candidate list; the polygon test provides the confirmation.
static Newtonoid2d *FindClosestObjectInCell(World2d *world, const Cell *cell, Vector2d click_local_coords, Vector2d max_distance, char *log,
                                            size_t log_size, int *log_offset)
{
    if (!cell)
    {
        return NULL;
    }

    float shortest_dist_squared = VectorDistanceSquared_2d(ZERO_VECTOR_2D, max_distance);
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

        EntityId obj_id = obj->id;
        if (obj_id != cell_id)
        {
            LOG_ERROR("Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. ID in CELL = %d. ID in ENTITY = %d.\n", cell_id, obj_id);
            continue;
        }

        Surface2d surface = obj->surface;
        Vector2d vertice_offset = obj->anchor_position;
        bool click_in_object = IsPointInPolygon(click_local_coords,
                                                (Vector2d *)surface.surface_vectors.items,
                                                vertice_offset,
                                                surface.surface_vectors.count);

        float click_to_obj_dist_squared = VectorDistanceSquared_2d(click_local_coords, obj->anchor_position);
        if (click_to_obj_dist_squared < shortest_dist_squared && click_in_object)
        {
            shortest_dist_squared = click_to_obj_dist_squared;
            closest = obj;
        }

        AppendLogLine(log, log_size, log_offset, "[ID:%d POS:%.1f,%.1f] ", i + 1, obj_id, obj->anchor_position.x, obj->anchor_position.y);
    }

    return closest;
}

// Resolve a click to the closest entity in the click's world-local grid cell.
Newtonoid2d *ResolveClosestEntityAt(World2d *active_world, Vector2d click_local_coords,
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

// Initialise gameplay-wide world state and the command queue used by its UI.
void InitWorldSystem(void)
{
    UIState_SetSelection(NULL, NULL, -1);
    if (G_UIState.newtonoid_params)
    {
        Deallocate((void **)&G_UIState.newtonoid_params,
                   sizeof(*G_UIState.newtonoid_params));
    }
    G_UIState.newtonoid_params = AllocateBytes(sizeof(Newtonoid2dParams));
    extern void InitCommandQueue(void);
    InitCommandQueue();
}

// Advance every running world, or advance each paused world once when a debug
// step was requested, then refresh selected-entity state after array changes.
static void UpdateWorldSimulation(bool step_requested)
{
    ProcessCommandQueue();

    for (int world_index = 0; world_index < G_Universe.world_count; world_index++)
    {
        World2d *world = &G_Universe.worlds[world_index];
        if (world->mode != PAUSED || step_requested)
        {
            UpdateWorld(world, frame_counter.delta_time);
        }
    }

    UIState_ValidateSelection();
}

// Apply debug keyboard actions that directly manipulate the selected world.
static void HandleWorldDebugHotkeys(World2d *active_world, Vector2d click_world_coords)
{
    if (!active_world)
    {
        return;
    }

    // Default debug spawn values used by the keyboard test controls.
    float radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.8f);
    float mass = radius * polygonoid_mass_default;
    int vertice_count = 7;
    Vector2d velocity = {
        .x = GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8),
        .y = GetRandomFloat(polygonoid_velocity_default.y * 8, polygonoid_velocity_default.y * -8)};
    Vector2d acceleration = polygonoid_acceleration_default;
    ColourRgba colour = polygonoid_line_colour;

    if (IsKeyPressed(KEY_SPACE))
    {
        TogglePause(active_world);
    }

    if (IsKeyPressed(KEY_ONE))
    {
        CreateAddNewtonoid(vertice_count, radius, SHAPE_BUILD_IRREGULAR,
                           mass, colour, click_world_coords,
                           velocity, acceleration);
        UpdateWorld(active_world, frame_counter.delta_time);
    }
}

// Fire from the selected entity when input routing has not already been claimed.
static InputRouteResult TryHandleFireProjectile(World2d *active_world,
                                                InputRouteResult prior_result)
{
    if (!active_world || prior_result != INPUT_ROUTE_IGNORED || !IsKeyPressed(KEY_F))
    {
        return prior_result;
    }

    Newtonoid2d *shooter = UIState_GetSelectedObject();
    int shooter_world_index = -1;
    Universe_GetEntityByID(&G_Universe,
                           shooter ? shooter->id : INVALID_ENTITY_ID,
                           &shooter_world_index);

    World2d *shooter_world = Universe_GetWorld(&G_Universe, shooter_world_index);
    if (shooter && shooter_world == active_world && shooter_world_index >= 0 &&
        !IsProjectile(shooter))
    {
        if (FireProjectile(shooter_world, shooter, PROJECTILE_TYPE_BOLT) != INVALID_ENTITY_ID)
        {
            return INPUT_ROUTE_HANDLED;
        }
    }

    return prior_result;
}

// Resolve and capture the closest selectable entity for drag operations.
static InputRouteResult TrySelectAndCaptureEntity(World2d *active_world,
                                                  const InputFrame *input,
                                                  Vector2d click_world_coords,
                                                  DragInteractionState *game_drag_ctx,
                                                  InputRouteResult prior_result)
{
    if (!active_world || !input || !game_drag_ctx ||
        prior_result != INPUT_ROUTE_IGNORED || !input->left_down ||
        game_drag_ctx->has_capture)
    {
        return prior_result;
    }

    char log[256] = "";
    int offset = 0;
    AppendLogLine(log, sizeof(log), &offset,
                  "WORLD (%.1f,%.1f) --> ",
                  game_viewport.local_origin.x,
                  game_viewport.local_origin.y);

    Cell *selected_cell = NULL;
    int selected_cell_index = -1;
    Newtonoid2d *closest = ResolveClosestEntityAt(active_world,
                                                  click_world_coords,
                                                  &selected_cell,
                                                  &selected_cell_index,
                                                  log,
                                                  sizeof(log),
                                                  &offset);

    UIState_SetSelection(closest, selected_cell, selected_cell_index);

    if (closest)
    {
        SnapshotDraggedEntityMotion(closest);
        DragInteraction_BeginCapture(game_drag_ctx,
                                     DRAG_TARGET_WORLD_ENTITY,
                                     closest,
                                     closest->anchor_position);
        AppendLogLine(log, sizeof(log), &offset,
                      "--> SELECTED ENTITY: ID:%d", closest->id);
    }
    else
    {
        g_drag_motion_snapshot.entity = NULL;
        g_drag_motion_snapshot.has_snapshot = false;
        DragInteraction_ClearCapture(game_drag_ctx);
        AppendLogLine(log, sizeof(log), &offset,
                      " --> SELECTED ENTITY: NULL");
    }

    LOG_INFO("CLICKED (%d,%d) | %s\n",
             (int)input->pointer_position.x,
             (int)input->pointer_position.y,
             log);

    return closest ? INPUT_ROUTE_CAPTURED : INPUT_ROUTE_HANDLED;
}

// Move a dragged entity inside its current world and keep its space map in sync.
static void HandleIntraWorldEntityDrag(Newtonoid2d *dragged,
                                       World2d *source_world,
                                       DragInteractionState *game_drag_ctx)
{
    Vector2d previous_snapped_aabb_verts[4] = {0};
    CalcSnappedAABB_Vertices(dragged->surface.surface_vectors.items,
                             dragged->surface.surface_vectors.count,
                             dragged->anchor_position,
                             source_world->grid_space.space.frame.basis,
                             previous_snapped_aabb_verts);

    Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(
        previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);

    Vector2d initial_world_coords = ResolvePixelToWorldFrame(
        source_world, game_drag_ctx->pointer_state.initial_pos);
    Vector2d current_world_coords = ResolvePixelToWorldFrame(
        source_world, game_drag_ctx->pointer_state.current_pos);
    Vector2d drag_delta_world = VectorDiff_2d(current_world_coords,
                                              initial_world_coords);
    Vector2d new_center = VectorSum_2d(game_drag_ctx->target_anchor,
                                       drag_delta_world);

    Space2d *space = &source_world->grid_space.space;
    Vector2d min_bound = space->grid_origin;
    Vector2d max_bound = {
        min_bound.x + (float)space->columns - 0.001f,
        min_bound.y + (float)space->rows - 0.001f};

    new_center.x = ClampFloat(new_center.x, min_bound.x, max_bound.x);
    new_center.y = ClampFloat(new_center.y, min_bound.y, max_bound.y);

    dragged->anchor_position = new_center;
    dragged->bounds_origin = (Vector2d){
        new_center.x - (dragged->bounds_size.x * 0.5f),
        new_center.y - (dragged->bounds_size.y * 0.5f)};
    dragged->velocity = ZERO_VECTOR_2D;
    dragged->acceleration = ZERO_VECTOR_2D;
    dragged->momentum = ZERO_VECTOR_2D;

    RemapEntityInASpace(&source_world->grid_space.space,
                        dragged,
                        previous_snapped_aabb_box,
                        &source_world->entity_space_map);
}

// Queue transfer when a dragged entity crosses into a different world.
static void HandleInterWorldEntityDrag(Newtonoid2d *dragged,
                                       World2d *destination_world,
                                       int source_world_index,
                                       int destination_world_index,
                                       Vector2d uni_coords,
                                       DragInteractionState *game_drag_ctx)
{
    Vector2d current_world_coords = ResolvePixelToWorldFrame(
        destination_world, game_drag_ctx->pointer_state.current_pos);

    // Re-seed the drag origin after a world transfer so the destination
    // world uses the current pointer position.
    game_drag_ctx->pointer_state.initial_pos = game_drag_ctx->pointer_state.current_pos;
    game_drag_ctx->pointer_state.previous_pos = game_drag_ctx->pointer_state.current_pos;

    Vector2d pointer_px = game_drag_ctx->pointer_state.current_pos;
    LOG_INFO("DRAG_TRANSFER enqueue: entity=%d src_world=%d dst_world=%d pointer_px=(%.2f,%.2f) uni=(%.2f,%.2f)\n",
             dragged->id,
             source_world_index,
             destination_world_index,
             pointer_px.x,
             pointer_px.y,
             uni_coords.x,
             uni_coords.y);

    EnqueueMoveEntity(dragged->id,
                      source_world_index,
                      destination_world_index,
                      destination_world->grid_space.object.id,
                      current_world_coords,
                      g_drag_motion_snapshot.collision_mask);
}

// Process pointer drag updates for a captured world entity.
static void HandleCapturedEntityDrag(DragInteractionState *game_drag_ctx)
{
    if (!game_drag_ctx || !game_drag_ctx->has_capture ||
        game_drag_ctx->target_kind != DRAG_TARGET_WORLD_ENTITY ||
        !DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
    {
        return;
    }

    Newtonoid2d *dragged = (Newtonoid2d *)game_drag_ctx->target;
    int source_world_index = -1;
    if (dragged)
    {
        Universe_GetEntityByID(&G_Universe, dragged->id, &source_world_index);
    }

    World2d *source_world = Universe_GetWorld(&G_Universe, source_world_index);
    Vector2d uni_coords = ResolvePixelToWorldFrame(&G_Universe.root_world,
                                                    game_drag_ctx->pointer_state.current_pos);

    int destination_world_index = Universe_FindWorldAt(&G_Universe, uni_coords);
    if (destination_world_index < 0)
    {
        destination_world_index = UNIVERSE_ROOT_WORLD_INDEX;
    }

    World2d *destination_world = Universe_GetWorld(&G_Universe,
                                                   destination_world_index);
    if (!dragged || !source_world || !destination_world)
    {
        return;
    }

    if (source_world_index == destination_world_index)
    {
        HandleIntraWorldEntityDrag(dragged, source_world, game_drag_ctx);
    }
    else
    {
        HandleInterWorldEntityDrag(dragged,
                                   destination_world,
                                   source_world_index,
                                   destination_world_index,
                                   uni_coords,
                                   game_drag_ctx);
    }
}

// Finish drag capture and restore the entity's physics state on mouse release.
static InputRouteResult TryHandleEntityDragRelease(const InputFrame *input,
                                                   DragInteractionState *game_drag_ctx,
                                                   InputRouteResult prior_result)
{
    if (!input || !game_drag_ctx || !input->left_released ||
        game_drag_ctx->pointer_state.left_button_hold_ticks <= 0)
    {
        return prior_result;
    }

    if (game_drag_ctx->has_capture &&
        game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY)
    {
        RestoreDraggedEntityMotion((Newtonoid2d *)game_drag_ctx->target);
        return INPUT_ROUTE_HANDLED;
    }

    return prior_result;
}

// Route gameplay input for simulation, world-entity selection, dragging, and firing.
InputRouteResult UpdateWorldSystem(const InputFrame *input, InputRouteResult prior_result)
{
    if (!input)
    {
        return prior_result;
    }

    // Right Arrow advances every paused world through the normal simulation
    // path exactly once for this input frame.
    UpdateWorldSimulation(IsKeyPressed(KEY_RIGHT));

    bool cursor_in_game_viewport = ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position);

    // Fall back to the universe's root world so unworlded entities remain selectable.
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    if (!active_world)
    {
        active_world = &G_Universe.root_world;
    }
    Vector2d click_pixel_coords = input->pointer_position;
    Vector2d click_world_coords = ResolvePixelToWorldFrame(active_world, click_pixel_coords);

    // Pick root-owned entities in universe coordinates when the pointer is outside nested worlds.
    if (prior_result == INPUT_ROUTE_IGNORED && cursor_in_game_viewport && input->left_down)
    {
        Vector2d click_universe_coords = ResolvePixelToWorldFrame(&G_Universe.root_world,
                                                                  click_pixel_coords);
        bool root_entity_hit = ResolveClosestEntityAt(&G_Universe.root_world,
                                                       click_universe_coords,
                                                       NULL, NULL, NULL, 0, NULL) != NULL;
        int clicked_world_index = Universe_FindWorldAt(&G_Universe, click_universe_coords);
        if (root_entity_hit || clicked_world_index < 0)
        {
            active_world = &G_Universe.root_world;
            click_world_coords = click_universe_coords;
        }
        else
        {
            active_world = Universe_GetWorld(&G_Universe, clicked_world_index);
            click_world_coords = ResolvePixelToWorldFrame(active_world, click_pixel_coords);
        }
    }

    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);

    if (active_world)
    {
        HandleWorldDebugHotkeys(active_world, click_world_coords);
        prior_result = TryHandleFireProjectile(active_world, prior_result);

        if (prior_result == INPUT_ROUTE_IGNORED && cursor_in_game_viewport && input->left_down)
        {
            prior_result = TrySelectAndCaptureEntity(active_world,
                                                     input,
                                                     click_world_coords,
                                                     game_drag_ctx,
                                                     prior_result);
            HandleCapturedEntityDrag(game_drag_ctx);
        }
        else
        {
            prior_result = TryHandleEntityDragRelease(input, game_drag_ctx,
                                                      prior_result);
        }
    }

    return prior_result;
}

// Create and add a debug Newtonoid using the requested regular or irregular shape.
void CreateAddNewtonoid(int vertice_count, float radius, ShapeBuildType build_type,
                        float mass, ColourRgba colour, Vector2d anchor_position,
                        Vector2d velocity, Vector2d acceleration)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    if (!active_world)
    {
        return;
    }

    Newtonoid2d new_newtonoid = {0};

    switch (build_type)
    {
    case SHAPE_BUILD_REGULAR:
        new_newtonoid = CreateNewtonoid2d_Symmetric(vertice_count, radius, colour, mass, anchor_position, velocity, acceleration);
        break;
    case SHAPE_BUILD_IRREGULAR:
        float min_radius = GetRandomFloat(0, radius);
        new_newtonoid = CreateNewtonoid2d_Irregular(vertice_count, min_radius, radius, colour, mass, anchor_position, velocity, acceleration);
        break;
    default:
        break;
    }

    if (new_newtonoid.radius > 0.0f)
    {
        Newtonoid_ConfigureMetadata(&new_newtonoid, FLAG_TYPE_NEWTONOID,
                                     FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE,
                                     FLAG_ATTR_RIGID | FLAG_ATTR_DAMAGEABLE,
                                     FLAG_STATUS_ALIVE,
                                     COLOUR_GAME_INK_RGBA, colour);
        Newtonoid_ConfigureHealth(&new_newtonoid, 3.0f);
        AddObjectToWorld(active_world, &new_newtonoid, active_world->grid_space.object.id);
    }
}

// Toggle the simulation state of a world without changing editing state.
void TogglePause(World2d *world)
{
    if (world->mode == RUNNING || world->mode == PAUSED)
    {
        world->mode = (world->mode == RUNNING) ? PAUSED : RUNNING;
    }
}

// Convert editor creation parameters into an allocated Newtonoid entity.
Newtonoid2d *ResolveEntityParamsToEntity(Newtonoid2dParams *newtonoid_params)
{
    if (!newtonoid_params)
    {
        return NULL;
    }

    ShapeType shape_type = newtonoid_params->shape_type;
    int vertice_count = newtonoid_params->vertice_count;

    if (shape_type == SHAPE_AUTO)
    {
        if (vertice_count == 3)
        {
            shape_type = SHAPE_TRIANGLE;
        }
        else if (vertice_count == 4)
        {
            shape_type = SHAPE_SQUARE;
        }
        else
        {
            shape_type = SHAPE_POLYGON;
        }
    }

    switch (shape_type)
    {
    case SHAPE_TRIANGLE:
        vertice_count = 3;
        break;
    case SHAPE_SQUARE:
    case SHAPE_RECTANGLE:
        vertice_count = 4;
        break;
    case SHAPE_CIRCLE:
        vertice_count = MAX_SHAPE_VERTICES;
        break;
    case SHAPE_ROTOR:
        break;
    case SHAPE_GEAR:
        break;
    case SHAPE_POLYGON:
        break;
    default:
        return NULL;
    }

    if (shape_type != SHAPE_ROTOR && vertice_count < 3)
    {
        LOG_WARN("Invalid vertice_count: vertice_count = %d\n", vertice_count);
        return NULL;
    }
    if (shape_type == SHAPE_ROTOR &&
        (vertice_count < 2 || vertice_count > MAX_SHAPE_VERTICES / 6))
    {
        LOG_WARN("Invalid rotor blade count: blade_count = %d\n", vertice_count);
        return NULL;
    }
    if (newtonoid_params->width <= 0.0 || newtonoid_params->height <= 0.0)
    {
        LOG_WARN("Invalid size: width = %f, height = %f\n", newtonoid_params->width, newtonoid_params->height);
        return NULL;
    }

    if (shape_type == SHAPE_ROTOR)
    {
        Newtonoid2d rotor = CreateNewtonoid2d_Rotor(
            vertice_count,
            (Vector2d){newtonoid_params->width, newtonoid_params->height},
            newtonoid_params->mass,
            newtonoid_params->anchor_position,
            newtonoid_params->velocity,
            newtonoid_params->acceleration);
        if (!rotor.surface.surface_vectors.items || rotor.surface.surface_vectors.count < 3)
        {
            ClearLArray(&rotor.surface.surface_vectors);
            return NULL;
        }

        Newtonoid2d *rotor_object = AllocateBytes(sizeof(Newtonoid2d));
        if (!rotor_object)
        {
            ClearLArray(&rotor.surface.surface_vectors);
            LOG_WARN("Failed to allocate new physical object. World entity pool full.\n");
            return NULL;
        }

        *rotor_object = rotor;
        Newtonoid_ConfigureRestitution(rotor_object, newtonoid_params->restitution);
        Newtonoid_ConfigureFriction(rotor_object, newtonoid_params->friction);
        LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
                 rotor_object->id, shape_type, rotor_object->anchor_position.x, rotor_object->anchor_position.y);
        return rotor_object;
    }

    if (shape_type == SHAPE_GEAR)
    {
        Newtonoid2d gear = CreateNewtonoid2d_Gear(
            vertice_count,
            (Vector2d){newtonoid_params->width, newtonoid_params->height},
            newtonoid_params->mass,
            newtonoid_params->anchor_position,
            newtonoid_params->velocity,
            newtonoid_params->acceleration);
        if (!gear.surface.surface_vectors.items || gear.surface.surface_vectors.count < 3)
        {
            ClearLArray(&gear.surface.surface_vectors);
            return NULL;
        }

        Newtonoid2d *gear_object = AllocateBytes(sizeof(Newtonoid2d));
        if (!gear_object)
        {
            ClearLArray(&gear.surface.surface_vectors);
            LOG_WARN("Failed to allocate new physical object. World entity pool full.\n");
            return NULL;
        }

        *gear_object = gear;
        Newtonoid_ConfigureRestitution(gear_object, newtonoid_params->restitution);
        Newtonoid_ConfigureFriction(gear_object, newtonoid_params->friction);
        LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
                 gear_object->id, shape_type, gear_object->anchor_position.x, gear_object->anchor_position.y);
        return gear_object;
    }

    Surface2d surface = {0};
    if (shape_type == SHAPE_SQUARE || shape_type == SHAPE_RECTANGLE)
    {
        surface = CreateSurface_Rectangular(
            (Vector2d){newtonoid_params->width, newtonoid_params->height});
    }
    else
    {
        surface.surface_vectors = CreateVertices_Symmetric(
            vertice_count, newtonoid_params->width * 0.5f,
            newtonoid_params->height * 0.5f);
    }

    Newtonoid2d *obj = CreateNewtonoid2d_Reference(
        newtonoid_params->mass, newtonoid_params->anchor_position,
        newtonoid_params->velocity, newtonoid_params->acceleration,
        surface);
    if (!obj)
    {
        LOG_WARN("Failed to allocate new physical object. World entity pool full.\n");
        return NULL;
    }
    obj->shape_type = shape_type;
    Newtonoid_ConfigureRestitution(obj, newtonoid_params->restitution);
    Newtonoid_ConfigureFriction(obj, newtonoid_params->friction);

    LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
             obj->id, shape_type, obj->anchor_position.x, obj->anchor_position.y);

    return obj;
}

// Return the number of live and clocked entities in the selected world.
int GetNewtonoidCount(void)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    return active_world ? active_world->objects.count + active_world->temp_objects.count : 0;
}

// Release the shared job-system resources when the gameplay screen unloads.
void UnloadGameplayScreen(void)
{
    ShutdownJobSystem();
}
