/**********************************************************************************************
 *
 * WORLD LIFECYCLE AND SIMULATION
 *
 * World construction, destruction, and frame updates. Gameplay input lives in
 * src/engine/system/world_system.c, while collision response lives in world_physics.c.
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "world/cell_walls.h"
#include "physics/physics.h"
#include "system/entities/relation_system.h"
#include "system/entities/rotor_system.h"
#include "system/entities/gear_system.h"
#include "system/entities/portal_system.h"
#include "system/job_system.h"
#include "entities/entity_registry.h"
#include "system/frame_profiler.h" // PROFILER

// Number of delayed world commands initially reserved for a new world.
static int initObjectCount = 4;

// True when the cell at (col, row) is BLOCKED: outside the grid, absent (given back to the
// root world), or present and SOLID. A face shared with a blocked neighbour is never exposed,
// and a blocked out-of-grid neighbour stops a border wall pushing entities into the container.
static bool Cell_IsBlockedAt(const Space2d *space, Cell *cells, int col, int row)
{
    return CellWalls_IsBlocked(cells, space->columns, space->rows, col, row);
}

// Test and resolve every BLOCKED cell (absent, or present+solid) against the entities the
// broad phase already placed in it, treating each as a STATIC (immovable) wall. Absent cells
// are implicit SOLID tiles: the world's shape is its set of present cells.
//
// This mirrors ResolveCollision_ContainerRect: the entity's tight AABB is compared
// with the axis-aligned cell box [grid_origin + (col, row), + 1], the least-
// penetration exposed face is chosen, and one fixed axis-aligned normal is passed
// to ResolveCollision_WithRotation. Faces shared with another blocked cell are
// skipped so seams between neighbours never produce internal collisions.
//
// A single reusable scratch Newtonoid2d (`cell_body`) is the static partner: its
// anchor is moved to each cell's centre and mass == 0 gives inverse_mass == 0, so
// only the entity moves. It is never registered in the entity system or spatial
// map, and the Cell struct is never written to (it stays a POD).
static void ResolveSolidCellCollisions(World2d *world, const CellOccupancy *occupancy,
                                       Cell *cells, Space2d *space, Newtonoid2d *cell_body,
                                       float gravity, float delta_time)
{
    int columns = space->columns;
    if (columns < 1)
        return; // No grid layout; nothing to collide against.

    // Walk the occupied-cell blocks (first-touch order). SNAPSHOT RULE: no pointer into the store is
    // held across any call. The block count is read once, and each block's count and ids are copied
    // into locals before the inner loop. The resolution calls below do not mutate the store today,
    // but the snapshot keeps this pass correct even if a callee ever Adds (pool growth would
    // invalidate block pointers).
    int n_blocks = CellOccupancy_BlockCount(occupancy);
    for (int b = 0; b < n_blocks; b++)
    {
        const OccupancyBlock *block = CellOccupancy_BlockAt(occupancy, b);
        int cell_i = block->cell;
        int block_count = block->count;

        // An emptied cell has no occupants, so skip it before any further work.
        if (block_count < 1)
            continue;
        EntityId ids[MAX_CELL_OCCUPANCY];
        for (int k = 0; k < block_count; k++)
            ids[k] = block->ids[k];

        // Gate: only blocked cells act as walls, i.e. absent OR present+SOLID. Flags are
        // read only for present cells, so a stale SOLID bit on an absent cell is moot.
        int row = cell_i / columns;
        int col = cell_i % columns;
        if (!Cell_IsBlockedAt(space, cells, col, row))
            continue;

        // Build the axis-aligned unit box from the grid origin plus (col, row) and centre
        // the scratch body on it. A per-cell local origin is NOT used: it would include the basis
        // and so disagree with GetIndexFromCoords under sheared or scaled bases.
        Vector2d cell_min = {space->grid_origin.x + (float)col, space->grid_origin.y + (float)row};
        Vector2d cell_max = {cell_min.x + 1.0f, cell_min.y + 1.0f};
        cell_body->anchor_position = (Vector2d){cell_min.x + 0.5f, cell_min.y + 0.5f};

        // Resolve every entity the broad phase placed in this cell against the
        // static cell box. An entity spanning several solid cells is resolved
        // against each overlapped cell in turn (no merged colliders).
        for (int occupant = 0; occupant < block_count; occupant++)
        {
            EntityId obj_id = ids[occupant];
            if (obj_id < 1)
                continue;

            Newtonoid2d *entity = (Newtonoid2d *)GetEntityByID(world, obj_id);
            if (!entity || !EntityIsEligbleForSpatialMap(entity))
                continue;

            // Gates matching the wall and pair paths: world-owned entities only,
            // role masks in both directions, and no-contact bodies never respond.
            if (entity->parent_id != world->grid_space.object.id)
                continue;
            if (!(entity->collision_role_mask & cell_body->roles) ||
                !(cell_body->collision_role_mask & entity->roles))
                continue;
            if (entity->constraints & ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE)
                continue;

            // Recompute the world vertices and tight AABB now: earlier cell
            // corrections moved the anchor, so the cached bounds may be stale.
            Vector2d verts[MAX_SHAPE_VERTICES] = {0};
            int count = (int)entity->surface.surface_vectors.count;
            if (count < 2)
                continue; // CalcAABBCoords_Tight needs at least two vertices.
            if (count > MAX_SHAPE_VERTICES)
                count = MAX_SHAPE_VERTICES;
            Newtonoid_TransformVertices(entity, verts, MAX_SHAPE_VERTICES);
            Matrix2x2 box = CalcAABBCoords_Tight(verts, count, ZERO_VECTOR_2D);
            Vector2d e_min = box.col1;
            Vector2d e_max = box.col2;

            // Detection: strict AABB overlap against the cell box.
            if (!(e_max.x > cell_min.x && e_min.x < cell_max.x &&
                  e_max.y > cell_min.y && e_min.y < cell_max.y))
                continue;

            // Per axis, pick the side from the AABB centre against the cell centre
            // and measure the full distance needed to clear that face.
            Vector2d push_x, push_y;
            float depth_x, depth_y;
            if ((e_min.x + e_max.x) < (cell_min.x + cell_max.x))
            {
                push_x = (Vector2d){-1.0f, 0.0f};
                depth_x = e_max.x - cell_min.x;
            }
            else
            {
                push_x = (Vector2d){1.0f, 0.0f};
                depth_x = cell_max.x - e_min.x;
            }
            if ((e_min.y + e_max.y) < (cell_min.y + cell_max.y))
            {
                push_y = (Vector2d){0.0f, -1.0f};
                depth_y = e_max.y - cell_min.y;
            }
            else
            {
                push_y = (Vector2d){0.0f, 1.0f};
                depth_y = cell_max.y - e_min.y;
            }

            // A face is exposed only when its neighbouring cell is not blocked.
            bool x_exposed = !Cell_IsBlockedAt(space, cells, col + (int)push_x.x, row);
            bool y_exposed = !Cell_IsBlockedAt(space, cells, col, row + (int)push_y.y);

            // Choose the exposed face with the least penetration; an interior cell
            // (no exposed face) is skipped rather than pushing through it.
            Vector2d push_normal;
            float depth;
            if (x_exposed && (!y_exposed || depth_x <= depth_y))
            {
                push_normal = push_x;
                depth = depth_x;
            }
            else if (y_exposed)
            {
                push_normal = push_y;
                depth = depth_y;
            }
            else
            {
                continue;
            }

            // Contact is the vertex deepest into the cell; the resolver expects the
            // normal pointing from the entity towards the cell.
            Vector2d contact_point = FindDeepestVertex(verts, (size_t)count, push_normal, true);
            Vector2d normal_to_cell = VectorScale_2d(push_normal, -1.0f);
            float support = CalcSupportNormalImpulse(entity, normal_to_cell,
                                                     (Vector2d){0.0f, gravity}, delta_time);
            ResolveCollision_WithRotation(entity, cell_body, normal_to_cell,
                                          contact_point, depth, support);
        }
    }
}
// Recovery pass: move every entity whose CENTRE lies in an ABSENT cell to the centre of the
// nearest open (present, non-solid) cell. The solid-cell pass cannot free an entity buried in
// a hole of 2x2 or larger (no exposed face), and a tunnelled entity may end up in a hole, so
// this guarantees nothing stays walled in. Entity-major, O(entities).
//
// Skipped: entities with no collision role (currently dragged), no-contact bodies (portals),
// and attached children (they ride their parent). Out-of-grid centres are the container's
// concern and are left alone. Does nothing when the grid has no open cell.
static void RecoverEntitiesFromAbsentCells(World2d *world, CellOccupancy *occupancy,
                                           Cell *cells, Space2d *space)
{
    Newtonoid2d *entity = NULL;
    LArray_ForEach(&world->objects, Newtonoid2d *, entity)
    {
        // Eligibility gates: alive world-owned physical bodies that take part in contacts.
        if (!EntityIsEligbleForSpatialMap(entity) ||
            entity->parent_id != world->grid_space.object.id ||
            entity->collision_role_mask == ENTITY_ROLE_NONE ||
            (entity->constraints & ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE))
            continue;

        // Only act when the centre is inside the grid but in an absent cell.
        int cell_index = GetIndexFromCoords(space, entity->anchor_position);
        if (cell_index < 0 || Cell_IsPresent(space, cell_index))
            continue;

        int from_col = cell_index % space->columns;
        int from_row = cell_index / space->columns;
        int to_col, to_row;
        if (!CellWalls_FindNearestOpen(cells, space->columns, space->rows,
                                       from_col, from_row, &to_col, &to_row))
            return; // No open cell anywhere; nothing to recover into.

        // Remember the old snapped footprint so the spatial map can be updated.
        Vector2d old_vertices[MAX_SHAPE_VERTICES] = {0};
        Newtonoid_TransformVertices(entity, old_vertices, MAX_SHAPE_VERTICES);
        Vector2d old_snapped_verts[4] = {0};
        CalcSnappedAABB_Vertices(old_vertices, entity->surface.surface_vectors.count,
                                 ZERO_VECTOR_2D, old_snapped_verts);
        Matrix2x2 old_snapped_box = CalcAABBCoords_Tight(old_snapped_verts, 4, ZERO_VECTOR_2D);

        // Relocate to the target cell's centre and refresh the bounds from the new pose.
        Vector2d old_centre = entity->anchor_position;
        entity->anchor_position = (Vector2d){space->grid_origin.x + (float)to_col + 0.5f,
                                             space->grid_origin.y + (float)to_row + 0.5f};
        Vector2d new_vertices[MAX_SHAPE_VERTICES] = {0};
        UpdateEntityBounds(entity, new_vertices);

        // Remove only the velocity component heading back towards the wall it came from.
        Vector2d to_wall = VectorDiff_2d(old_centre, entity->anchor_position);
        float wall_dist = sqrtf(to_wall.x * to_wall.x + to_wall.y * to_wall.y);
        if (wall_dist > 0.0f)
        {
            Vector2d n = {to_wall.x / wall_dist, to_wall.y / wall_dist};
            float into_wall = entity->velocity.x * n.x + entity->velocity.y * n.y;
            if (into_wall > 0.0f)
            {
                entity->velocity.x -= into_wall * n.x;
                entity->velocity.y -= into_wall * n.y;
            }
        }

        RemapEntityInASpace(space, entity, old_snapped_box, occupancy);
        LOG_INFO("Recovered entity %d from absent cell %d to cell (%d,%d).\n",
                 entity->id, cell_index, to_col, to_row);
    }
}

// Compose the world-local, universe-camera, and viewport transforms for rendering.
Matrix3x3 ResolveWorldToPixelMatrix(const World2d *world, const Camera2d *universe_camera)
{
    if (!world || !universe_camera)
    {
        return IDENTITY_MATRIX_3x3;
    }

    // Compose the viewport, camera, and supplied world-local transform.
    Matrix3x3 camera_to_pixel = MatrixMultiply_3x3_3x3(
        game_viewport.tunnel.source_to_dest_mtx,
        universe_camera->tunnel.source_to_dest_mtx);
    return MatrixMultiply_3x3_3x3(camera_to_pixel, world->tunnel.source_to_dest_mtx);
}

// Invert the world-to-pixel transform for screen-to-world input conversion.
Matrix3x3 ResolvePixelToWorldMatrix(const World2d *world, const Camera2d *universe_camera)
{
    return MatrixInvert_3x3(ResolveWorldToPixelMatrix(world, universe_camera));
}

// Initialise a world and allocate its entity, collision, spatial, and command storage.
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
    out_world->grid_space.object.id = EntityRegistry_AllocateId();

    if (out_world->grid_space.object.id == INVALID_ENTITY_ID)
    {
        LOG_ERROR("Cannot create world: entity ID allocation failed.\n");
        return false;
    }
    // Register the world entity in the entity registry.
    if (!EntityRegistry_SetLocation(out_world->grid_space.object.id, out_world->grid_space.object.id, -1))
    {
        EntityRegistry_ReleaseId(out_world->grid_space.object.id);
        LOG_ERROR("Cannot create world: entity location registration failed.\n");
        return false;
    }

    // Ancestry wiring: every frame-bearing node links to its parent via parent_id so a single
    // uniform upward walk reaches the tree root. The universe's root_world is that root, so a
    // nested world's parent is the root world's id, while the root world itself terminates the
    // chain with INVALID_ENTITY_ID. (A world body's parent_id has no physics/render side effect:
    // all parent_id consumers iterate world->objects, which never contains a world body.)
    bool is_root_world = (out_world == &universe->root_world);
    out_world->grid_space.object.parent_id =
        is_root_world ? INVALID_ENTITY_ID : universe->root_world.grid_space.object.id;

    // Tag the world id as a WORLD node so ancestry/UI can classify it (worlds and plain
    // entities are otherwise indistinguishable via EntityRegistry_GetEntity). Presence is the
    // signal; the tag stores no world state. A failure here is non-fatal to world creation.
    WorldComponent world_tag = {0};
    if (!EntityRegistry_AttachWorld(out_world->grid_space.object.id, &world_tag))
    {
        LOG_WARN("World %d: WorldComponent tag attach failed; ancestry classification degraded.\n",
                 out_world->grid_space.object.id);
    }
    // Use the creation settings belonging to this universe, not global system state.
    int object_count = universe->next_object_count;
    if (object_count < 0)
    {
        object_count = 0;
    }
    out_world->objects = MakeLArray(object_count, sizeof(Newtonoid2d));
    out_world->collisions = MakeLArray(object_count, sizeof(Matrix2x2));

    // Initialise per-world spatial, collision, and delayed-command storage.
    // Sparse occupancy store. A failed initial Reserve is non-fatal (Add grows lazily). The Reset is
    // mandatory: it stamps cell_limit, without which every spawn would be refused as out of range.
    out_world->cell_occupancy = (CellOccupancy){0};
    if (!CellOccupancy_Reserve(&out_world->cell_occupancy, CELL_OCCUPANCY_INITIAL_BLOCKS))
        LOG_ERROR("Cell occupancy store initial allocation failed; it will grow lazily.\n");
    CellOccupancy_Reset(&out_world->cell_occupancy, (int)space_obj.space.cells.count);
    // The old expression 1 + entity_space_map.count / 2 always evaluated to capacity 1 (count starts at 0).
    out_world->resolved_collisions = MakeFlatMapInt(1);
    out_world->scheduled_world_cmds = MakeLArray(initObjectCount, sizeof(WorldCommand));
    InitJobSystem(256);
    return true;
}

// Release all allocations owned by a world and clear its state.
void DestroyWorld(World2d *world)
{
    if (!world)
    {
        return;
    }

    DestroyWorldEntityStorage(world);
    ClearLArray(&world->collisions);
    ClearLArray(&world->scheduled_world_cmds);
    ClearDArray(&world->grid_space.space.cells);
    CellOccupancy_Free(&world->cell_occupancy);
    ClearFlatMapInt(&world->resolved_collisions);
    MemorySet(world, 0, sizeof(*world));
}

// Advance one world through scheduled commands, physics integration, attachment
// updates, spatial remapping, and broad-phase collision-pair processing.
void UpdateWorld(World2d *world, float delta_time)
{
    LArray *objects = &world->objects;
    GridSpace2d *space_entity = &world->grid_space;
    Space2d *space = &space_entity->space;

    // Cache the per-frame maps and command storage used by the update passes.
    CellOccupancy *occupancy = &world->cell_occupancy;
    FlatMapInt *resolved_collisions = &world->resolved_collisions;
    LArray *scheduled_world_cmds = &world->scheduled_world_cmds;

    // Run delayed world events first so deletions continue even when no
    // inhabitants remain in the world.
    RunScheduledWorldCmds(scheduled_world_cmds, world);

    // Update all systems in order (before physics).
    RelationSystem_Update(world);  // Validate all relations before systems interpret them
    RotorSystem_Update(world);
    GearSystem_Update(world);
    PortalSystem_Update(world);

    int obj_count = objects->count;
    if (obj_count < 1)
        return;

    PROFILE_BEGIN(PROF_RESET); // PROFILER
    LArray_Reset(&world->collisions);
    ResetFlatMapInt(resolved_collisions);

    // Rewind the sparse occupancy store: O(occupied cells), independent of grid area.
    CellOccupancy_Reset(occupancy, (int)space->cells.count);
    // Reserve AFTER Reset so a steady-state tick never allocates in the job. Heuristic: up to four
    // blocks per entity, capped by the grid size. A failure is non-fatal (Add retries lazily).
    int reserve_blocks = obj_count * 4;
    if (reserve_blocks > (int)space->cells.count)
        reserve_blocks = (int)space->cells.count;
    if (!CellOccupancy_Reserve(occupancy, reserve_blocks) && CellOccupancy_TakeAllocFailureReport(occupancy))
        LOG_ERROR("Cell occupancy store allocation failed. Some entities are not tracked spatially this tick.\n");
    PROFILE_END(PROF_RESET); // PROFILER
    Cell *cells = (Cell *)space->cells.items;

    if (!IsJobSystemInitialized())
    {
        InitJobSystem(256);
    }
    ClearJobs();
    PROFILE_BEGIN(PROF_PHYSICS); // PROFILER
    SubmitJob(PhysicsUpdateJob, world, obj_count, 8);
    ExecuteJobs();
    PROFILE_END(PROF_PHYSICS); // PROFILER
    ClearJobs();

    // Pass 1 is performed by PhysicsUpdateJob: integrate positions and
    // velocities, refresh AABBs, and insert each entity into the spatial grid.

    // Pass 2 resolves attachment hierarchies after parent simulation and before
    // collision processing, then remaps each moved child in the spatial grid.
    Newtonoid2d *child;
    PROFILE_BEGIN(PROF_REMAP); // PROFILER
    LArray_ForEach(objects, Newtonoid2d *, child)
    {
        if (!(child->status_flags & ENTITY_STATUS_FLAG_ALIVE) || child->parent_id == space_entity->object.id ||
            child->parent_id == INVALID_ENTITY_ID)
        {
            continue;
        }

        // Resolve the parent through the generation-checked global slot map.
        Newtonoid2d *parent = (Newtonoid2d *)GetEntityByID(world, child->parent_id);
        if (!parent)
        {
            child->parent_id = INVALID_ENTITY_ID;
            continue;
        }

        Vector2d previous_world_vertices[MAX_SHAPE_VERTICES] = {0};
        Newtonoid_TransformVertices(child, previous_world_vertices, MAX_SHAPE_VERTICES);
        Vector2d previous_snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(previous_world_vertices,
                     child->surface.surface_vectors.count,
                     ZERO_VECTOR_2D,
                     previous_snapped_aabb_verts);
        Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(
            previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);

        // Glue the child's world position to the parent's new position plus its offset.
        child->anchor_position = VectorSum_2d(parent->anchor_position, child->parent_offset);
        child->bounds_origin = VectorSum_2d(parent->bounds_origin, child->bounds_origin);
        RemapEntityInASpace(space, child, previous_snapped_aabb_box,
                            occupancy);
    }
    PROFILE_END(PROF_REMAP); // PROFILER

    // Pass 3a resolves entities against PRESENT+SOLID tile cells (static walls).
    // Build the transient scratch collider ONCE and reuse it for every solid
    // cell, then dispose its heap surface LArray ONCE after the pass (see below).
    // This runs for a single entity too, so a lone entity still bounces off a
    // solid cell, so it is placed before the entity-pair guard (which needs >= 2).
    if (obj_count >= 1)
    {
        // A centred unit box matching the cell quad. CreateSurface_Rectangular
        // allocates surface_vectors via MakeLArray (a value LArray with a heap buffer),
        // so the owning scratch body must release it with ClearLArray once, after the pass.
        Surface2d cell_surface = CreateSurface_Rectangular((Vector2d){1.0f, 1.0f});
        // mass 0 => inverse_mass 0 => immovable, exactly like a world boundary wall.
        Newtonoid2d cell_body = CreateNewtonoid2d(0.0f, ZERO_VECTOR_2D, ZERO_VECTOR_2D,
                                                  ZERO_VECTOR_2D, cell_surface);
        // Mirror the default wall contract so the same entities (newtonoids and
        // projectiles) that collide with real walls also collide with solid cells.
        cell_body.roles = ENTITY_ROLE_WALL;
        cell_body.collision_role_mask = ENTITY_DEFAULT_COLLISION_ROLE_MASK;
        cell_body.status_flags |= ENTITY_STATUS_FLAG_ALIVE; // A valid, awake collision partner.
        // Inner walls use the container's (user-editable) material so they feel like the outer wall.
        cell_body.restitution = world->grid_space.object.restitution;
        cell_body.friction = world->grid_space.object.friction;

        PROFILE_BEGIN(PROF_SOLID); // PROFILER
        ResolveSolidCellCollisions(world, occupancy, cells, space, &cell_body,
                                   world->gravity, delta_time);
        // Safety net: relocate anything left with its centre inside an absent cell.
        RecoverEntitiesFromAbsentCells(world, occupancy, cells, space);
        PROFILE_END(PROF_SOLID); // PROFILER

        // Single release of the scratch body's vertex buffer. surface_vectors is a VALUE
        // LArray embedded in the stack-local cell_body (made via MakeLArray), so only its
        // internal buffer is heap-owned. ClearLArray frees that buffer and leaves the
        // header alone; DisposeLArray would free() the stack address (undefined behaviour).
        ClearLArray(&cell_body.surface.surface_vectors);
    }

    // Pass 3b resolves entity pairs sharing at least one broad-phase cell.
    if (obj_count < 2)
    {
        return;
    }

    // RESOLVE ENTITY-ENTITY COLLISIONS
    PROFILE_BEGIN(PROF_PAIRS); // PROFILER
    // SNAPSHOT RULE: no pointer into the store is held across a call. The block count is read once and
    // each block's count and ids are copied into locals before the pair loops. ProcessCollisionPair
    // can call AddObjectToWorld for collision markers; those are ROLE_EFFECT so they never enter the
    // store today, but the snapshot makes correctness independent of that (pool growth would
    // otherwise invalidate block pointers).
    int n_blocks = CellOccupancy_BlockCount(occupancy);
    for (int b = 0; b < n_blocks; b++)
    {
        const OccupancyBlock *block = CellOccupancy_BlockAt(occupancy, b);
        int cell_i = block->cell;
        int cell_occ = block->count;

        if (cell_occ >= 2)
        {
            EntityId ids[MAX_CELL_OCCUPANCY];
            for (int k = 0; k < cell_occ; k++)
                ids[k] = block->ids[k];

            for (int m = 0; m < cell_occ; m++)
            {
                // Test every unique entity pairing in this cell. The physics
                // module deduplicates pairs found in multiple cells.
                for (int n = m + 1; n < cell_occ; n++)
                {
                    ProcessCollisionPair(world, ids[m], ids[n], cell_i,
                                         resolved_collisions, scheduled_world_cmds);
                }
            }
        }
    }

    // Post-pair container pass: pile pressure from the pair pass can push bottom-layer bodies back into
    // the walls, so correct them in the same frame (part of the PROF_PAIRS phase). This runs AFTER the
    // recovery pass above by design: a body clamped back inside may land in an absent cell, which the
    // recovery pass relocates at the start of next frame's solid phase (before any pair work).
    ResolveContainerPass(world);
    PROFILE_END(PROF_PAIRS); // PROFILER
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
//    FlatMapInt_GetValue(context.entity_location_index, a->parent_id, &parent_index);

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