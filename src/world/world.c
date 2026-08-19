/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "events/events.h"
#include "physics/physics.h"
#include "system/job_system.h"
#include "editor/geometry_editor.h"
#include "system/command_queue.h"
#include "system/ui_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

static int initObjectCount = 4;

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

bool CreateWorld(GridSpace2d space_obj, float gravity, struct Universe *universe, World2d *out_world)
{
    if (!out_world || !universe)
    {
        LOG_ERROR("Cannot create world without output storage and a universe owner.\n");
        return false;
    }

    out_world->grid_space = space_obj;
    out_world->gravity = gravity;
    out_world->universe = universe;
    out_world->flags = WORLD_FLAG_ACTIVE | WORLD_FLAG_VISIBLE | WORLD_FLAG_SELECTABLE |
                       WORLD_FLAG_PHYSICS_ENABLED | WORLD_FLAG_SPAWNS_ENABLED |
                       WORLD_FLAG_DRAGGABLE;
    if (universe)
    {
        out_world->grid_space.object.id = Universe_AllocateEntityId(universe);
    }

    if (out_world->grid_space.object.id == INVALID_ENTITY_ID)
    {
        LOG_ERROR("Cannot create world: universe entity ID allocation failed.\n");
        return false;
    }
    int object_count = *GetNextWorldObjectCountPtr();
    if (object_count < 0)
    {
        object_count = 0;
    }
    out_world->objects = MakeLArray(object_count, sizeof(Newtonoid2d));
    out_world->collisions = MakeLArray(object_count, sizeof(Matrix2x2));
    out_world->temp_objects = MakeLArray(object_count, sizeof(Newtonoid2d));

    // INIT WORLD INTERNAL STATE (PER-WORLD)
    out_world->entity_space_map = MakeFlatMapInt(1 + (int)(space_obj.space.cells.count / 5));
    out_world->resolved_collisions = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
    out_world->entity_world_index_registry = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
    out_world->scheduled_world_cmds = MakeLArray(initObjectCount, sizeof(WorldCommand));
    InitJobSystem(256);
    return true;
}

EntityId AddObjectToWorld(World2d *world, Newtonoid2d *object, EntityId parent_id)
{
    // Object placement is center-based; grid occupancy still snaps that center into a cell index.
    Vector2d local_coords = object->anchor_position;
    Space2d *space = &world->grid_space.space;
    int grid_cell_index = GetIndexFromCoords(space, local_coords);
    if (grid_cell_index < 0)
    {
        LOG_WARN("Desired spawn point (%0.2f,%0.2f) out of bounds. Cannot add entity to the world.\n", local_coords.x, local_coords.y);
        return INVALID_ENTITY_ID; // Click is outside the structural world viewport boundaries! Avoid resolving cell.
    }

    // Solid objects are collision-enabled, need to be tracked spacially
    int cell_index = -1;
    if (!(object->entity_flags & FLAG_TYPE_EFFECT))
    {
        cell_index = grid_cell_index;
        // Add the object's ID to the cell's object_ids array if there is space, and update the object's footprint based on its surface and the coordinate space's basis vectors.
        // We also need to update the occupancy of the cell and ensure that we don't exceed the maximum
        Cell *cells = world->grid_space.space.cells.items;
        Cell *target_cell = &cells[cell_index];
        if (target_cell->occupancy >= MAX_CELL_OCCUPANCY)
        {
            LOG_WARN("Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
            return INVALID_ENTITY_ID;
        }
    }

    object->parent_id = parent_id;

    // Register only after all placement checks pass so failed adds leave no stale entity.
    EntityId assigned_id = RegisterEntity(world, object);
    if (assigned_id == INVALID_ENTITY_ID)
    {
        return INVALID_ENTITY_ID;
    }

    if (cell_index >= 0)
    {
        Cell *target_cell = &((Cell *)world->grid_space.space.cells.items)[cell_index];
        target_cell->object_ids[target_cell->occupancy] = object->id;
        target_cell->occupancy++;
    }

    LOG_INFO("CREATED OBJECT (ID %d): Cell %d : Center(%.1f, %.1f)\n", object->id, cell_index, local_coords.x, local_coords.y);
    return assigned_id;
}

// Process a single collision pair and handle collision response
// Returns true if collision was processed, false if skipped
bool ProcessCollisionPair(World2d *world, EntityId obj_id_a, EntityId obj_id_b, int cell_i, FlatMapInt *resolved_collisions, LArray *scheduled_world_cmds)
{
    // Validate entity IDs
    if (obj_id_a < 1 || obj_id_b < 1)
    {
        LOG_ERROR("Could not find objects with IDs %d and %d in Cell (index = %d).\n", obj_id_a, obj_id_b, cell_i);
        return false;
    }

    // Check if this collision pair was already resolved
    unsigned long obj_pair_hash_key = CalcHashFromInts(obj_id_a, obj_id_b);
    short is_resolved = 0;
    if (FlatMapInt_GetValue(resolved_collisions, obj_pair_hash_key, (int *)&is_resolved))
        return false;

    Newtonoid2d *a = GetEntityByID(world, obj_id_a);
    Newtonoid2d *b = GetEntityByID(world, obj_id_b);
    if (!a || !b)
    {
        LOG_WARN("Collision pair references missing entities %d and %d in Cell (index = %d).\n",
                 obj_id_a, obj_id_b, cell_i);
        return false;
    }

    // Check collision masks for compatibility
    if (!(a->collision_mask & b->entity_flags) || !(b->collision_mask & a->entity_flags))
        return false;

    // Run SAT collision detection
    CollisionResult_SAT collision_result = CheckForCollision_SAT(a, b);
    if (!collision_result.is_colliding)
        return false;

    // Record collision
    LArray_Push(&world->collisions, &collision_result.collision_box);
    LOG_INFO("COLLISION detected between Object ID %d and Object ID %d Coord Box Range: [%0.2f,%0.2f] [%0.2f,%0.2f] \n",
             obj_id_a, obj_id_b, collision_result.collision_box.col1.x, collision_result.collision_box.col1.y,
             collision_result.collision_box.col2.x, collision_result.collision_box.col2.y);

    // Resolve collision
    ResolveCollision(a, b);

    // Mark as resolved
    FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);

    // Create debug visualization object
    Newtonoid2d *penetrating_entity = collision_result.penetrating_entity;
    Matrix2x2 collision_box = collision_result.collision_box;
    Vector2d collision_center = CalcGeometricCentre_FromBox(collision_box);
    Vector2d dimensions = {collision_box.col2.x - collision_box.col1.x, collision_box.col2.y - collision_box.col1.y};
    Vector2d collision_vertices_arr[4];
    CalcBoxVertices(dimensions, ZERO_VECTOR_2D, collision_vertices_arr);
    Surface2d collision_surface = {0};
    collision_surface.surface_vectors = MakeLArray(4, sizeof(Vector2d));
    MemoryCopy(collision_surface.surface_vectors.items, collision_vertices_arr, sizeof(collision_vertices_arr));
    collision_surface.surface_vectors.count = 4;
    Newtonoid2d collision_obj = CreateNewtonoid2d(0.00001f, collision_center, penetrating_entity->velocity,
                                                  penetrating_entity->acceleration, collision_surface);
    collision_obj.entity_flags = FLAG_TYPE_EFFECT;
    collision_obj.status_flags |= FLAG_LIFETIME_CLOCKED;
    StickEntity(world, &collision_obj, penetrating_entity);
    EntityId id = AddObjectToWorld(world, &collision_obj, penetrating_entity->id);

    // Schedule deletion of debug object
    if (id != INVALID_ENTITY_ID)
    {
        ScheduleEntityDeletion(scheduled_world_cmds, id, FLAG_STATUS_ALIVE, 120, 1, 1);
    }

    return true;
}

void UpdateWorld(World2d *world, float delta_time)
{
    // PrintCurrentBytesAlloc();
    LArray *objects = &world->objects;
    GridSpace2d *space_entity = &world->grid_space;
    Space2d *space = &space_entity->space;

    // RESET TRACKING-STATE - Zero out
    FlatMapInt *entity_space_map = &world->entity_space_map;
    FlatMapInt *resolved_collisions = &world->resolved_collisions;
    FlatMapInt *entity_world_index_registry = &world->entity_world_index_registry;
    LArray *scheduled_world_cmds = &world->scheduled_world_cmds;

    // RUN SCHEDULED WORLD EVENTS first so delayed deletions continue even when no inhabitants remain.
    RunScheduledWorldCmds(scheduled_world_cmds, world);

    int obj_count = objects->count + world->temp_objects.count;
    if (obj_count < 1)
        return;

    LArray_Reset(&world->collisions);
    ResetFlatMapInt(entity_space_map);
    ResetFlatMapInt(resolved_collisions);

    // Optimized: Only reset cells in active world region instead of entire universe grid
    // This reduces O(universe_cells) to O(world_cells), typically 3600 -> 12 iterations
    Cell *cells = space_entity->space.cells.items;
    int grid_width = space->columns;
    int grid_height = space->rows;

    // Reset only cells within the world's active region (bounds are in world-local coordinates)
    for (int row = 0; row < grid_height; row++)
    {
        for (int col = 0; col < grid_width; col++)
        {
            int cell_index = row * space->columns + col;
            Cell *target_cell = &cells[cell_index];
            target_cell->occupancy = 0;
            MemorySet(&cells[cell_index].object_ids, 0, sizeof(cells[cell_index].object_ids));
        }
    }

    if (!IsJobSystemInitialized())
    {
        InitJobSystem(256);
    }
    ClearJobs();
    SubmitJob(PhysicsUpdateJob, world, obj_count, 8);
    ExecuteJobs();
    ClearJobs();

    // PASS 1: Simulating Independent Physics
    // Update object positions based on their velocity and acceleration, then update the cells they occupy in the coordinate space grid as well as the entity_space_map which tracks how many objects occupy each cell (for collision checking later)
    // The physics work is now split into jobs for better task separation and future parallelism.

    // NOTE: The loop body is executed via PhysicsUpdateJob. No inline loop here anymore.

    LArray *temp_objects = &world->temp_objects;
    // PASS 2: Resolving Attachment Hierarchies
    Newtonoid2d *child;
    LArray_ForEach(temp_objects, Newtonoid2d *, child)
    {
        if (!(child->status_flags & FLAG_STATUS_ALIVE) || child->parent_id == space_entity->object.id)
            continue;

        // Look up where the parent currently lives in memory using the registry
        int parent_packed_loc = 0;
        if (!FlatMapInt_GetValue(entity_world_index_registry, child->parent_id, &parent_packed_loc))
        {
            // The parent was likely deleted! Detach or kill the child so it doesn't crash
            child->parent_id = INVALID_ENTITY_ID;
            continue;
        }

        // Unpack the routing info to pull the parent out of the correct array
        int parent_type = UNPACK_INT_HIGH(parent_packed_loc);
        int parent_idx = UNPACK_INT_LOW(parent_packed_loc);
        LArray *parent_array = (parent_type == ARCHETYPE_CLOCKED) ? temp_objects : objects;

        Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(parent_array, parent_idx);
        if (!parent)
        {
            child->parent_id = INVALID_ENTITY_ID;
            continue;
        }

        Vector2d previous_snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(child->surface.surface_vectors.items,
                                 child->surface.surface_vectors.count,
                                 child->anchor_position,
                                 space->frame.basis,
                                 previous_snapped_aabb_verts);
        Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(
            previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);

        // Glue the child's world position to the parent's new position + offset
        child->anchor_position = VectorSum_2d(parent->anchor_position, child->parent_offset);
        child->bounds_origin = VectorSum_2d(parent->bounds_origin, child->bounds_origin);
        RemapEntityInASpace(space, child, previous_snapped_aabb_box,
                            entity_space_map);
    }
    // Check for collisions
    if (obj_count < 2) // early return
    {
        // ClearFlatMapInt(&entity_space_map);
        // PrintCurrentBytesAlloc();
        return;
    }

    // RESOLVE ENTITY-ENTITY COLLISIONS
    for (size_t i = 0; i < entity_space_map->capacity; i++)
    {
        if (entity_space_map->slots[i].key == 0 && entity_space_map->slots[i].value == 0)
            continue;

        int cell_i = entity_space_map->slots[i].key;
        int cell_occ = 0;
        FlatMapInt_GetValue(entity_space_map, cell_i, &cell_occ);

        if (cell_occ >= 2)
        {
            Cell *cell = &cells[cell_i];
            for (size_t m = 0; m < cell_occ; m++)
            {
                // Check against all possible object pairings out of all object occupants in this cell
                for (size_t n = m + 1; n < cell_occ; n++)
                {
                    EntityId obj_id_a = cell->object_ids[m];
                    EntityId obj_id_b = cell->object_ids[n];
                    ProcessCollisionPair(world, obj_id_a, obj_id_b, cell_i, resolved_collisions, scheduled_world_cmds);
                }
            }
        }
    }
}




/**********************************************************************************************
 *

 *
 **********************************************************************************************/
#include <stdint.h>
#include <stdarg.h>
#include "raylib.h"
#include "system/utility_system.h"
#include "world/world.h"
#include "world/universe.h"
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
#include "input/drag_interaction.h"
#include "system/debug_overlay_system.h"
// #include "screens.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
// ----------WORLD SCREEN----------
static int finishScreen = 0;
bool world_grid_debug_labels_enabled = false;
// Game-region placement in logical screen units.
// Vector2d game_viewport_local_origin, game_viewport_local_end = {0};
// Vector2d game_viewport_local_resolution = {0};
float gravity = 10;
// Objects and properties
static ColourRgba polygonoid_line_colour = {194, 105, 83, 255};
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
    uint32_t collision_mask;
    bool has_snapshot;
} EntityDragMotionSnapshot;

static EntityDragMotionSnapshot g_drag_motion_snapshot = {0};
//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
// void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region);
void CreateAddNewtonoid(int vertice_count, float radius, ShapeBuildType build_type,
                        float mass, ColourRgba colour, Vector2d anchor_position,
                        Vector2d velocity, Vector2d acceleration);
void TogglePause(World2d *world);

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

Vector2d ResolvePixelToWorldFrame(const World2d *active_world, Vector2d pixel_coords)
{
    if (!active_world)
    {
        return ZERO_VECTOR_2D;
    }

    Matrix3x3 game_viewport_inv_transform = game_viewport.tunnel.dest_to_source_mtx; // pixel to viewport
    Matrix3x3 parent_inv_transform = G_Universe.camera.tunnel.dest_to_source_mtx;    // viewport to universe
    Matrix3x3 child_inv_transform = active_world->tunnel.dest_to_source_mtx;         // universe to world-local

    Vector2d viewport_coords = TransformCoordinates(game_viewport_inv_transform, pixel_coords);
    Vector2d parent_coords = TransformCoordinates(parent_inv_transform, viewport_coords);
    Vector2d child_coords = TransformCoordinates(child_inv_transform, parent_coords);

    return child_coords;
    // LOG_INFO("ResolvePixelToWorldFrame: pixel_coords=(%.2f, %.2f) -> viewport_coords=(%.2f, %.2f) -> parent_coords=(%.2f, %.2f) -> child_coords=(%.2f, %.2f)\n",
    //          pixel_coords.x, pixel_coords.y,
    //          viewport_coords.x, viewport_coords.y,
    //          parent_coords.x, parent_coords.y,
    //          child_coords.x, child_coords.y);
    // return TransformCoordinates(input_to_child_local_mtx, click_parent_coords);
}

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

        Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(obj->anchor_position, -1), click_local_coords);
        float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
        if (click_to_obj_mag < shortest_dist && click_in_object)
        {
            shortest_dist = click_to_obj_mag;
            closest = obj;
        }

        AppendLogLine(log, log_size, log_offset, "[ID:%d POS:%.1f,%.1f] ", i + 1, obj_id, obj->anchor_position.x, obj->anchor_position.y);
    }

    return closest;
}

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

// FIRST: Initialisation of Gameplay Screen
void InitWorldSystem(void)
{
    // Init Global World State
    UIState_SetSelection(NULL, NULL, -1);
    G_UIState.newtonoid_params = AllocateBytes(sizeof(Newtonoid2dParams));
    // Initialise command queue for UI->World commands
    extern void InitCommandQueue(void);
    InitCommandQueue();
}

static void UpdateWorldSimulation(void)
{
    ProcessCommandQueue();

    for (int world_index = 0; world_index < G_Universe.world_count; world_index++)
    {
        World2d *world = &G_Universe.worlds[world_index];
        if (world->mode != PAUSED)
        {
            UpdateWorld(world, frame_counter.delta_time);
        }
    }

    // Re-resolve selected entity pointer after simulation mutations to avoid stale references.
    UIState_ValidateSelection();
}

// Gameplay Screen Update logic
InputRouteResult UpdateWorldSystem(const InputFrame *input, InputRouteResult prior_result)
{
    if (!input)
    {
        return prior_result;
    }

    UpdateWorldSimulation();

    int mouse_x = (int)input->pointer_position.x;
    int mouse_y = (int)input->pointer_position.y;

    bool cursor_in_game_viewport = mouse_x >= game_viewport.pixel_origin.x && mouse_x <= (game_viewport.pixel_origin.x + (game_viewport.pixel_u.x * game_viewport.resolution.x)) && mouse_y >= game_viewport.pixel_origin.y && mouse_y <= (game_viewport.pixel_origin.y + (game_viewport.pixel_v.y * game_viewport.resolution.y));

    // Fall back to the universe's own root world so unworlded entities remain selectable/draggable.
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    if (!active_world)
    {
        active_world = &G_Universe.root_world;
    }
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

        // DEBUGGING - 1 Frame setp-through
        if (IsKeyPressed(KEY_LEFT_SHIFT) && active_world->mode == PAUSED)
        {
            // PrintCurrentBytesAlloc();
            UpdateWorld(active_world, frame_counter.delta_time);
            // PrintCurrentBytesAlloc();
        }

        if (IsKeyPressed(KEY_ONE))
        {
            CreateAddNewtonoid(vertice_count, radius, SHAPE_BUILD_IRREGULAR, mass, colour, click_world_coords, velocity, acceleration);
            UpdateWorld(active_world, frame_counter.delta_time);
        }

        if (prior_result == INPUT_ROUTE_IGNORED && cursor_in_game_viewport && input->left_down)
        {
            if (!game_drag_ctx->has_capture) // Only attempt to select an entity if we don't already have a drag capture
            {
                char log[256] = "";
                int offset = 0;
                AppendLogLine(log, sizeof(log), &offset, "WORLD (%.1f,%.1f) --> ", game_viewport.local_origin.x, game_viewport.local_origin.y);

                Cell *selected_cell = NULL;
                int selected_cell_index = -1;
                Newtonoid2d *p_closest = ResolveClosestEntityAt(active_world, click_world_coords, &selected_cell, &selected_cell_index,
                                                                log, sizeof(log), &offset);

                // Update object/cell selection atomically so downstream UI readers see a coherent state.
                UIState_SetSelection(p_closest, selected_cell, selected_cell_index);

                if (p_closest)
                {
                    SnapshotDraggedEntityMotion(p_closest);
                    DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_ENTITY, p_closest, p_closest->anchor_position);
                    AppendLogLine(log, sizeof(log), &offset, "--> SELECTED ENTITY: ID:%d", p_closest->id);
                }
                else
                {
                    g_drag_motion_snapshot.entity = NULL;
                    g_drag_motion_snapshot.has_snapshot = false;
                    DragInteraction_ClearCapture(game_drag_ctx);
                    AppendLogLine(log, sizeof(log), &offset, " --> SELECTED ENTITY: NULL");
                }

                LOG_INFO("CLICKED (%d,%d) | %s\n", mouse_x, mouse_y, log);
                prior_result = p_closest ? INPUT_ROUTE_CAPTURED : INPUT_ROUTE_HANDLED;
            }

            if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY &&
                     DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
            {
                Newtonoid2d *dragged = (Newtonoid2d *)game_drag_ctx->target;
                // Look up by stable ID (O(worlds)) instead of scanning every entity by pointer.
                int source_world_index = -1;
                if (dragged)
                {
                    Universe_GetEntityByID(&G_Universe, dragged->id, &source_world_index);
                }
                World2d *source_world = Universe_GetWorld(&G_Universe, source_world_index);

                Vector2d viewport_coords = TransformCoordinates(game_viewport.tunnel.dest_to_source_mtx, game_drag_ctx->pointer_state.current_pos);
                Vector2d uni_coords = TransformCoordinates(G_Universe.camera.tunnel.dest_to_source_mtx, viewport_coords);

                int destination_world_index = Universe_FindWorldAt(&G_Universe, uni_coords);
                if (destination_world_index < 0)
                {
                    // No nested world under the cursor: drop back into the universe's root world.
                    destination_world_index = UNIVERSE_ROOT_WORLD_INDEX;
                }
                World2d *destination_world = Universe_GetWorld(&G_Universe, destination_world_index);
                if (dragged && source_world && destination_world)
                {
                    if (source_world_index == destination_world_index)
                    {
                        Vector2d previous_snapped_aabb_verts[4] = {0};
                        CalcSnappedAABB_Vertices(dragged->surface.surface_vectors.items,
                                                 dragged->surface.surface_vectors.count,
                                                 dragged->anchor_position, source_world->grid_space.space.frame.basis,
                                                 previous_snapped_aabb_verts);
                        Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);
                        Vector2d initial_world_coords = ResolvePixelToWorldFrame(source_world, game_drag_ctx->pointer_state.initial_pos);
                        Vector2d current_world_coords = ResolvePixelToWorldFrame(source_world, game_drag_ctx->pointer_state.current_pos);
                        Vector2d drag_delta_world = VectorSum_2d(current_world_coords, (Vector2d){-initial_world_coords.x, -initial_world_coords.y});
                        Vector2d new_center = VectorSum_2d(game_drag_ctx->target_anchor, drag_delta_world);
                        Space2d *space = &source_world->grid_space.space;
                        Vector2d min_bound = space->grid_origin;
                        Vector2d max_bound = {min_bound.x + (float)space->columns - 0.001f, min_bound.y + (float)space->rows - 0.001f};
                        new_center.x = fmaxf(min_bound.x, fminf(new_center.x, max_bound.x));
                        new_center.y = fmaxf(min_bound.y, fminf(new_center.y, max_bound.y));
                        dragged->anchor_position = new_center;
                        dragged->bounds_origin = (Vector2d){
                            new_center.x - (dragged->bounds_size.x * 0.5f),
                            new_center.y - (dragged->bounds_size.y * 0.5f)};
                        dragged->velocity = ZERO_VECTOR_2D;
                        dragged->acceleration = ZERO_VECTOR_2D;
                        dragged->momentum = ZERO_VECTOR_2D;
                        RemapEntityInASpace(&source_world->grid_space.space, dragged, previous_snapped_aabb_box,
                                            &source_world->entity_space_map);
                    }
                    else
                    {
                        Vector2d current_world_coords = ResolvePixelToWorldFrame(destination_world, game_drag_ctx->pointer_state.current_pos);
                        // Re-seed the drag origin so the next capture in the destination world starts from
                        // the current pointer position instead of reusing the original press location.
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
                        EnqueueMoveEntity(dragged->id, source_world_index, destination_world_index,
                                           destination_world->grid_space.object.id, current_world_coords,
                                           g_drag_motion_snapshot.collision_mask);
                    }
                }
            }
        }
        else if (input->left_released && game_drag_ctx->pointer_state.left_button_hold_ticks > 0)
        {
            if (game_drag_ctx->has_capture && game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY)
            {
                RestoreDraggedEntityMotion((Newtonoid2d *)game_drag_ctx->target);
                prior_result = INPUT_ROUTE_HANDLED;
            }
        }
    }

    return prior_result;
}

// Gameplay Screen Stage Update logic
// void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region)

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

    if (new_newtonoid.radius > 0.0)
    {
        new_newtonoid.line_colour = COLOUR_GAME_INK_RGBA;
        new_newtonoid.fill_colour = colour;
        AddObjectToWorld(active_world, &new_newtonoid, active_world->grid_space.object.id);
    }
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
    case SHAPE_POLYGON:
        break;
    default:
        return NULL;
    }

    if (vertice_count < 3)
    {
        LOG_WARN("Invalid vertice_count: vertice_count = %d\n", vertice_count);
        return NULL;
    }
    if (newtonoid_params->width <= 0.0 || newtonoid_params->height <= 0.0)
    {
        LOG_WARN("Invalid size: width = %f, height = %f\n", newtonoid_params->width, newtonoid_params->height);
        return NULL;
    }

    Surface2d surface = {0};
    if (shape_type == SHAPE_SQUARE || shape_type == SHAPE_RECTANGLE)
    {
        surface.surface_vectors = MakeLArray(vertice_count, sizeof(Vector2d));
        Vector2d box_vertices[4];
        CalcBoxVertices((Vector2d){newtonoid_params->width, newtonoid_params->height},
                        ZERO_VECTOR_2D, box_vertices);
        for (int i = 0; i < vertice_count; i++)
        {
            LArray_Push(&surface.surface_vectors, &box_vertices[i]);
        }
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

    LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
             obj->id, shape_type, obj->anchor_position.x, obj->anchor_position.y);

    return obj;
}
int GetNewtonoidCount(void)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    return active_world ? active_world->objects.count + active_world->temp_objects.count : 0;
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



        // DEBUGGING - Rapid firing of polygonoids
        // keyDown = IsKeyDown(KEY_ONE);
        // if (keyDown & frame_counter.total_frames % 3 == 0)
        // {
        //     radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.25);
        //     mass = radius * polygonoid_mass_default;
        //     velocity = (Vector2d){GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), GetRandomFloat(polygonoid_velocity_default.y * 0, polygonoid_velocity_default.y * 16)};
        //     Vector2d top_middle_world = (Vector2d){world.grid_space.object.bounds_size.x * 0.5, 0.3};
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

        // Vector2d ResolveEntityWorldCoords(Newtonoid2d *a, WorldState context)
// {
//    // Base Case 1: If this object has no parent, its local coordinates are its world coordinates
//    if (a->parent_id == 0)
//    {
//       return a->anchor_position;
//    }

//    int parent_index = 0;
//    FlatMapInt_GetValue(context.entity_world_index_registry, a->parent_id, &parent_index);

//    // Base Case 2: Parent ID exists but can't be found/resolved in registry
//    if (parent_index <= 0)
//    {
//       LOG_WARN("Parent object with ID %d could not be found in the entiy-world index registry in ResolveEntityWorldCoords.", a->parent_id);
//       return a->anchor_position;
//    }

//    Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(context.world_objects, parent_index);
//    if (parent == NULL)
//    {
//       return a->anchor_position;
//    }

//    // Recursion --> Go find the parent's absolute world coordinates first
//    Vector2d parent_world_coords = ResolveEntityWorldCoords(parent, context);

//    // Unwinding: Add this object's local offset to the parent's world position
//    return VectorSum_2d(parent_world_coords, a->anchor_position);
// }

//**********************************************************************************************
// {
//    ScheduledAction action = CreateScheduledAction(update_action_to_run, 1);

//     // Execute the function that was passed in!
//     update_action_to_run(event_data);

// }
// AxisIntersectionRange2d GetAxisCollisionRange(float min_a, float max_a, float min_b, float max_b, Vector2d unit_axis)
// {
//    // Even though u and v axis live in world space, the scalar results (minA, maxA) do not retain their independent X and Y identities.
//    // Once you perform the dot product operation, we have flattened a 2D coordinate into a single 1D number line
//    // You cannot reverse-engineer that single number back into a unique 2D point because you have discarded the perpendicular information.
//    // We need to convert the 1D scalar overlap region into the global X and Y world space axis from the u_unit_axis and v_unit_axis
//    // To get back to 2D world space coordinates, we aren't changing coordinate systems or multiplying by an inverse basis matrix.
//    // We are simply taking that 1D scalar boundary (d_start, d_end) line segment and un-flattening it back along the global vector direction: e.g., P_start = d_start * u_axis.
//    AxisIntersectionRange2d global_range = {0};

//    // Find the 1D scalar overlap region on the separating axis (u_axis or v_axis from SAT)
//    // The overlap starts at the maximum of the two minimums
//    float overlap_start = (min_a > min_b) ? min_a : min_b;
//    // The overlap ends at the minimum of the two maximums
//    float overlap_end = (max_b < max_b) ? max_b : max_b;

//    // If start >= end, there is no actual overlap happening on this axis
//    if (overlap_start >= overlap_end)
//    {
//       return global_range;
//    }

//    // Transform the 1D scalars back into 2D World Coordinate Space
//    // Since the axis vector is already in world space, we just scale it
//    global_range.start.x = overlap_start * unit_axis.x;
//    global_range.start.y = overlap_start * unit_axis.y;

//    global_range.end.x = overlap_end * unit_axis.x;
//    global_range.end.y = overlap_end * unit_axis.y;

//    return global_range;
// }
// Matrix2x2 CalcProjectedPoints_SAT(LArray vertices_arr, Vector2d vertice_offset, Vector2d u_unit_axis, Vector2d v_unit_axis) // Newtonoid2d *a, Newtonoid2d *b)
// {
//    // GET MAX AND MIN OF ALL VERTICES BY PROJECTING ONTO THE ABOVE AXIS (instead of the default x,y axis)
//    float min_x = INFINITY;
//    float max_x = -INFINITY;
//    float min_y = INFINITY;
//    float max_y = -INFINITY;

//    Vector2d *vertices = (Vector2d *)vertices_arr.items;
//    for (size_t j = 0; j < vertices_arr.count; j++)
//    {
//       Vector2d vertice = VectorSum_2d(vertices[j], vertice_offset); // Get vertice's world coordinates by adding the object's center coordinates to the vertice's local coordinates
//       // U AXIS
//       //  Angle between vertice_line and v_axis_unit is calculated by getting the difference in their angles in radians, then using that angle to get the magnitude of the vertice_line multiplied by the cosine of that angle to get the length of the vertice_line that is projected onto the v_axis_unit. This is equivalent to the dot product of the vertice_line and v_axis_unit.
//       Polar2d vertice_polar = PolarForm_2d(vertice);
//       float angle_diff = vertice_polar.radians - u_unit_axis.radians;
//       float vertice_proj_u_axis = vertice_polar.magnitude * cosf(angle_diff);

//       if (vertice_proj_u_axis > max_x)
//       {
//          max_x = vertice_proj_u_axis;
//       }
//       else if (vertice_proj_u_axis < min_x)
//       {
//          min_x = vertice_proj_u_axis;
//       }
//       printf("Vertice %zu projects to x = %0.2f on u_axis of line (%zu -> %zu). Radians = %0.2f\n", j, vertice_proj_u_axis, j, j + 1, angle_diff);

//       // V AXIS
//       // Angle between vertice_line and v_axis_unit is calculated by getting the difference in their angles in radians, then using that angle to get the magnitude of the vertice_line multiplied by the cosine of that angle to get the length of the vertice_line that is projected onto the v_axis_unit. This is equivalent to the dot product of the vertice_line and v_axis_unit.
//       angle_diff = vertice_polar.radians - v_unit_axis.radians;
//       float vertice_proj_v_axis = vertice_polar.magnitude * cosf(angle_diff);

//       if (vertice_proj_v_axis > max_y)
//       {
//          max_y = vertice_proj_v_axis;
//       }
//       else if (vertice_proj_v_axis < min_y)
//       {
//          min_y = vertice_proj_v_axis;
//       }

//       printf("Vertice %zu projects to y = %.2f on v_axis of line (%zu -> %zu). Radians = %0.2f\n", j, vertice_proj_v_axis, j, j + 1, angle_diff);
//    }
//    return (Matrix2x2){{min_x, max_x}, {min_y, max_y}};
// }

// void ResolvePenetration(Newtonoid2d *a, Newtonoid2d *b)
// {
//    // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
//    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
//    Vector2d a_b_pos_diff = VectorSum_2d(a->anchor_position, VectorScale_2d(b->anchor_position, -1.0f)); // distance vector between centers (From B to A)
//    float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
//    float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

//    // RESOLVE PENETRATION
//    if (a_b_pos_diff_mag < a_b_min_diff_mag)
//    {
//       // Calc scalar penetration depth
//       float penetration_depth = a_b_min_diff_mag - a_b_pos_diff_mag;

//       // Calc the collision normal unit vector
//       Vector2d normal = {0.0f, 0.0f};
//       if (a_b_pos_diff_mag > 0.0f)
//       {
//          normal = VectorScale_2d(a_b_pos_diff, 1.0f / a_b_pos_diff_mag);
//       }
//       else
//       {
//          // Edge case: Objects are perfectly stacked on top of each other. Pick an arbitrary up normal to push them apart.
//          normal = (Vector2d){0.0f, -1.0f};
//       }
//       // Distribute the overlapping vector between A and B to shift their positions (weighted inversely to their mass)
//       float total_inv_mass = a->inverse_mass + b->inverse_mass;
//       if (total_inv_mass <= 0.0f)
//          return; // both are static immovable objects, skip push out

//       // Calc Resolution Shifting Multipliers
//       float a_move_fraction = a->inverse_mass / total_inv_mass;
//       float b_move_fraction = b->inverse_mass / total_inv_mass;

//       // Total resolution vector required
//       Vector2d separation_vector = VectorScale_2d(normal, penetration_depth);

//       // A moves forward along the normal vector direction, B moves backward along the normal vector direction
//       a->anchor_position = VectorSum_2d(a->anchor_position, VectorScale_2d(separation_vector, a_move_fraction));
//       b->anchor_position = VectorSum_2d(b->anchor_position, VectorScale_2d(separation_vector, -b_move_fraction));
//    }
// }

// void ResolveVelocity(Newtonoid2d *a, Newtonoid2d *b)
// {
//    // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
//    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
//    Vector2d a_b_pos_diff = VectorSum_2d(a->anchor_position, VectorScale_2d(b->anchor_position, -1.0f)); // distance vector between centers (From B to A)
//    float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
//    float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

//    // RESOLVE VELOCITY
//    float total_inverse_mass = a->inverse_mass + b->inverse_mass;
//    if (total_inverse_mass <= 0.0f) // We have 2 immovable/static objects so just continue to the next collision
//    {
//       printf("WARNING: Both objects in this collision have infinite mass (0 inverse mass). No collision response applied.\n");
//       return;
//    }
//    // Apply momentum conservation to determine velocities of a and b
//    float a_mom_1 = VectorMagnitude_2d(a->velocity) / a->inverse_mass;
//    float b_mom_1 = VectorMagnitude_2d(b->velocity) / b->inverse_mass;
//    // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
//    Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos_diff, 1 / VectorMagnitude_2d(a_b_pos_diff)); // this is the unit-direction
//    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, a_b_pos_normal);                               // Velocity along the Normal (The Dot Product)

//    // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
//    // Calculate Impulse Scalar
//    float e = 1; // Coefficient of Restitution
//    float j = -(1 + e) * a_b_vel_dot / total_inverse_mass;

//    // Apply the Impulse
//    // Turn that scalar back into a vector and update the velocities
//    Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

//    // Apply the impulse vector to A and B to get velocities
//    Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->inverse_mass);
//    Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->inverse_mass);

//    a->velocity = VectorSum_2d(a->velocity, a_vel_change);
//    b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(b_vel_change, -1));
// }

// Need assign an area of effect (footprint), i.e. Snapped AABB, for the object based on its radius and update the occupancy of all cells that fall within that area
// otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision.
// Surface2d snapped_aabb = CalculateSnappedAABB(space->basis, object->surface, object->anchor_position);

// void UpdateWorld(World2d *world, float delta_time)
// {
//    LArray *objects = &world->objects; //.items;
//    int count = objects->count;
//    // 1. Update Object state first
//    // 1.1 Update Grid cells while we're here
//    if (count < 1)
//       return;

//    // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
//    Cell *cells = world->grid_space.space.cells.coll.items;
//    int cell_count = world->grid_space.space.cells.coll.count;
//    for (size_t i = 0; i < cell_count; i++)
//    {
//       Cell *target_cell = &cells[i];
//       target_cell->occupancy = 0;
//       memset(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
//    }

//    Polygonoid *polygonoids = objects->items;
//    for (size_t i = 0; i < count; i++)
//    {
//       Newtonoid2d *obj = &polygonoids[i].newtonian_properties;

//       // Ensure the object isn't outside the bounds of the world before we try to get the cell it's in, otherwise we could get an out of bounds error when we try to access the cell's object_ids array. We can just skip updating the cell for this object if it's out of bounds, but we should still update its vectors based on its acceleration and velocity so that it can move back into the bounds of the world.
//       if (obj->bounds_origin.x < 0 || obj->bounds_origin.x >= world->grid_space.space.resolution_ixj.x ||
//           obj->bounds_origin.y < 0 || obj->bounds_origin.y >= world->grid_space.space.resolution_ixj.y)
//       {
//          printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->bounds_origin.x, obj->bounds_origin.y);

//          // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore. For simplicity, let's just reverse the velocity of the object when it hits the boundary of the world, which will create a bouncing effect. We can also apply a damping factor to the velocity to simulate energy loss during the collision, which will prevent the object from bouncing indefinitely.
//          if (obj->bounds_origin.x < 0 || obj->bounds_origin.x >= world->grid_space.space.resolution_ixj.x)
//          {
//             obj->velocity.x = -obj->velocity.x; // Reverse and dampen the x velocity
//             // obj->bounds_origin.x = obj->bounds_origin.x < 0 ? 0 : world->grid_space.space.resolution_ixj.x - 1; // Move the object back within bounds
//          }
//          if (obj->bounds_origin.y < 0 || obj->bounds_origin.y >= world->grid_space.space.resolution_ixj.y)
//          {
//             obj->velocity.y = -obj->velocity.y; // Reverse and dampen the y velocity
//             // obj->bounds_origin.y = obj->bounds_origin.y < 0 ? 0 : world->grid_space.space.resolution_ixj.y - 1; // Move the object back within bounds
//          }
//          //Recalc inverse_mass in case mass was changed
//          obj->inverseMass = 1.0/obj->mass;
//          CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
//          continue;
//       }

//       // Add the object's ID to the cell's object_ids array if there is space
//       Cell *target_cell = GetCellFromCoords(&world->grid_space.space, polygonoids[i].newtonian_properties.bounds_origin);

//       if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
//       {
//          target_cell->object_ids[target_cell->occupancy] = polygonoids[i].id;
//          target_cell->occupancy++;
//       }
//       else
//       {
//          printf("WARNING: Cell (%d,%d) full. ID %d not tracked spatially.\n", target_cell->coords.x, target_cell->coords.y, polygonoids[i].id);
//          return;
//       }

//       // Update the object's vectors based on its current acceleration, velocity, and position, and the elapsed time since the last update
//       CalculateVectors(obj, delta_time);
//    }

//    // 2. Check for collisions
//    if (count < 2)
//       return;

//    //for(int i = 0; i <)
//    for (size_t i = 0; i < count; i++)
//    {
//       for (size_t j = i + 1; j < count; j++) // Optimized j loop
//       {
//          Polygonoid *a = &polygonoids[i];
//          Polygonoid *b = &polygonoids[j];

//          bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);

//          if (colliding)
//          {
//             // Apply momentum conservation to determine velocities of a and b
//             float a_mom_1 = VectorMagnitude_2d(a->newtonian_properties.velocity) / a->newtonian_properties.inverseMass;
//             float b_mom_1 = VectorMagnitude_2d(b->newtonian_properties.velocity) / b->newtonian_properties.inverseMass;
//             Vector2d a_b_vel = VectorSum_2d(a->newtonian_properties.velocity, VectorScale_2d(b->newtonian_properties.velocity, -1));

//             // Get the collision normal - just use A as the reference object
//             // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
//             Vector2d a_b_pos = VectorSum_2d(a->newtonian_properties.bounds_origin, VectorScale_2d(b->newtonian_properties.bounds_origin, -1));
//             Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos, 1 / VectorMagnitude_2d(a_b_pos));

//             // Velocity along the Normal (The Dot Product)
//             float a_b_vel_dot = VectorDot_2d(a_b_vel, a_b_pos_normal);

//             // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
//             // Calculate Impulse Scalar
//             float e = 1; // Coefficient of Restitution
//             float j = -(1 + e) * a_b_vel_dot / (a->newtonian_properties.inverseMass + b->newtonian_properties.inverseMass);

//             // Apply the Impulse
//             // Turn that scalar back into a vector and update the velocities
//             Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

//             // Apply the impulse vector to A and B to get velocities
//             Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->newtonian_properties.inverseMass);
//             Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->newtonian_properties.inverseMass);

//             a->newtonian_properties.velocity = VectorSum_2d(a->newtonian_properties.velocity, a_vel_change);
//             b->newtonian_properties.velocity = VectorSum_2d(b->newtonian_properties.velocity, VectorScale_2d(b_vel_change, -1));
//          }

//          frame_counter.total_frames % 300 == 0 ? printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
//                 a->newtonian_properties.bounds_origin.x,
//                 a->newtonian_properties.bounds_origin.y,
//                 b->newtonian_properties.bounds_origin.x,
//                 b->newtonian_properties.bounds_origin.y,
//                 colliding ? "TRUE" : "FALSE") : (void)0;

//       }
//    }
//    // UpdateObjectVectors(objs, delta_time);
//    // UpdateWorldState(objs, &world->grid_space.space, delta_time);
// }

// void UpdateWorldState(Collection *polygonoids, Space2d *space, float delta_time)
//{
//  // 1. Update Object state first
//  // 1.1 Update Grid cells while we're here
//  if (polygonoids->count < 1)
//     return;

// // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
// Cell *cells = space->cells.coll.items;
// for (size_t i = 0; i < space->cells.coll.count; i++)
// {
//    cells[i].occupancy = 0;
//    memset(cells[i].object_ids, 0, sizeof(cells[i].object_ids));
// }

// Polygonoid *pts = (Polygonoid *)polygonoids->items;
// for (size_t i = 0; i < polygonoids->count; i++)
// {
//    // NO COPYING. Point directly to the source in the heap.
//    CalculateVectors(&pts[i].newtonian_properties, delta_time);
//    // Add the object's ID to the cell's object_ids array if there is space

//    Cell *target_cell = GetCellFromCoords(space, pts[i].newtonian_properties.bounds_origin);

//    if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
//    {
//       object->id = world->next_object_id++;
//       target_cell->object_ids[target_cell->occupancy] = object->id;
//       target_cell->occupancy++;
//    }
//    else
//    {
//       printf("WARNING: Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
//       return;
//    }
// }

// // 2. Check for collisions
// if (polygonoids->count < 2)
//    return;

// for (size_t i = 0; i < polygonoids->count; i++)
// {
//    for (size_t j = i + 1; j < polygonoids->count; j++) // Optimized j loop
//    {
//       Polygonoid *a = &pts[i];
//       Polygonoid *b = &pts[j];

//       bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);

//       printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
//              a->newtonian_properties.bounds_origin.x,
//              a->newtonian_properties.bounds_origin.y,
//              b->newtonian_properties.bounds_origin.x,
//              b->newtonian_properties.bounds_origin.y,
//              colliding ? "TRUE" : "FALSE");
//    }
// }
//}

// void UpdateObjectsAndGrid(Collection *polygonoids, float delta_time)
// {
//    Polygonoid *pts = (Polygonoid *)polygonoids->items;

//    if (polygonoids->count < 1)
//       return;

//    for (size_t i = 0; i < polygonoids->count; i++)
//    {
//       // NO COPYING. Point directly to the source in the heap.
//       CalculateVectors(&pts[i].newtonian_properties, delta_time);
//    }
// }

// World CalculateFieldLines(Field field);
// World InitialiseFieldCells(Field field);