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
#include "world/contact_point.h"
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

// Default restitution of a world's container (its walls and solid cells). See CreateWorld.
#define WORLD_CONTAINER_DEFAULT_RESTITUTION 0.4f

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
// DETECT only: the entity's tight AABB is compared with the axis-aligned cell box
// [grid_origin + (col, row), + 1], the least-penetration exposed face is chosen, and one
// fixed axis-aligned normal becomes a cached STATIC contact (CONTACT_KIND_SOLID_CELL, partner =
// the solver's immovable body slot 0). Faces shared with another blocked cell are skipped so seams
// between neighbours never produce internal collisions.
//
// A solid cell is an ORDINARY static contact: the solver corrects it with the same soft,
// rate-limited NGS position step and the same velocity solve as any pair contact. (The old
// resolver also used a soft 20 %-of-excess push.) The container's full-depth AABB snap
// (ContactSolver_SnapToWall) is deliberately NOT applied here; that rule is for the container
// walls only. The material is the container's (user-editable), so inner walls feel like the outer
// one. The Cell struct is never written to (it stays a POD).
static void DetectSolidCellContacts(World2d *world, const CellOccupancy *occupancy,
                                    Cell *cells, Space2d *space)
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

        // Build the axis-aligned unit box from the grid origin plus (col, row). A per-cell local origin is NOT used: it would include the basis
        // and so disagree with GetIndexFromCoords under sheared or scaled bases.
        Vector2d cell_min = {space->grid_origin.x + (float)col, space->grid_origin.y + (float)row};
        Vector2d cell_max = {cell_min.x + 1.0f, cell_min.y + 1.0f};

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
            if (!(entity->collision_role_mask & ENTITY_ROLE_WALL) ||
                !(ENTITY_DEFAULT_COLLISION_ROLE_MASK & entity->roles))
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

            // Contact is the vertex deepest into the cell; the normal points from the entity towards the cell.
            Vector2d contact_point = StaticContactPoint(entity, verts, (size_t)count, push_normal, depth);
            Vector2d normal_to_cell = VectorScale_2d(push_normal, -1.0f);
            EmitStaticContact(world, entity, CONTACT_KIND_SOLID_CELL, (uint32_t)cell_i, normal_to_cell,
                              contact_point, depth);
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

        // Remember the old snapped footprint so the spatial map can be updated. This pass now runs AFTER the solve,
        // so the footprint is built from the entity's CURRENT (post-solve) pose, not a stale pre-solve box.
        Vector2d old_vertices[MAX_SHAPE_VERTICES] = {0};
        Newtonoid_TransformVertices(entity, old_vertices, MAX_SHAPE_VERTICES);
        Vector2d old_snapped_verts[4] = {0};
        CalcSnappedAABB_Vertices(old_vertices, entity->surface.surface_vectors.count,
                                 ZERO_VECTOR_2D, old_snapped_verts);
        Matrix2x2 old_snapped_box = CalcAABBCoords_Tight(old_snapped_verts, 4, ZERO_VECTOR_2D);

        // A relocated body leaves its neighbours: wake it (loud, so what rested on it is released) and note it
        // gone so their persistence records are delivered by the next wake pass.
        WakeUp(entity);
        ContactPersist_NoteGone(&world->contact_persist, entity->id);

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
    // The container (world walls, and solid cells, which share its material) gets a lower default
    // bounce than entities. Contact restitution is the geometric mean of both bodies, so a 0.9 entity
    // against a 0.4 wall bounces at about sqrt(0.9 * 0.4) = 0.6. That matches how bodies settled
    // before the iterative solver; the value stays user-editable per world in the state manager.
    Newtonoid_ConfigureRestitution(&out_world->grid_space.object, WORLD_CONTAINER_DEFAULT_RESTITUTION);
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
    // Contact cache for the detect/solve split. A failed initial Reserve is non-fatal: BeginFrame retries every tick
    // and the frame simply runs without contacts until it succeeds.
    out_world->contact_cache = (ContactCache){0};
    // Final settings (design 6.5): K = Kp = 4 sequential-impulse velocity iterations with warm start, the
    // restitution threshold, and the separate NGS position phase.
    out_world->contact_cache.config = ContactSolverConfig_Default();
    // Contact persistence set for the static-sleeper wake events. A failed initial Reserve is non-fatal:
    // Touch retries and a body whose pair cannot be tracked is simply never reported as new.
    out_world->contact_persist = (ContactPersist){0};
    out_world->contact_persist.grace = out_world->contact_cache.config.persist_grace;
    out_world->sleep_wake_scratch = (SleepWakeScratch){0};
    if (!ContactPersist_Reserve(&out_world->contact_persist, CONTACT_PERSIST_MIN_CAPACITY / 2))
        LOG_ERROR("Contact persistence set initial allocation failed; it will retry on first use.\n");
    if (!ContactCache_Reserve(&out_world->contact_cache, CONTACT_CACHE_MIN_CAPACITY, CONTACT_CACHE_MIN_CAPACITY, CONTACT_CACHE_MIN_CAPACITY))
        LOG_ERROR("Contact cache initial allocation failed; it will retry every tick.\n");
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
    ContactCache_Free(&world->contact_cache);
    ContactPersist_Free(&world->contact_persist);
    SleepWake_Free(&world->sleep_wake_scratch);
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
    // Contact cache: O(used) reset for this tick's detect phase (stores the step, gravity and restitution threshold).
    float physics_dt = WorldPhysicsDeltaTime();
    ContactCache_BeginFrame(&world->contact_cache, obj_count, physics_dt, world->gravity);
    world->contact_persist.grace = world->contact_cache.config.persist_grace;
    ContactPersist_BeginFrame(&world->contact_persist, physics_dt);
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

    // DETECT (static contacts): the container walls and the BLOCKED cells (absent, or present+solid)
    // become cached static contacts, emitted BEFORE the pairs so they survive a full cache and sit in one
    // index range. This runs for a single entity too, so a lone entity still bounces off a wall or solid
    // cell. Nothing is pushed or bounced here: the solver applies the response after detection.
    PROFILE_BEGIN(PROF_SOLID); // PROFILER
    DetectContainerContacts(world);
    DetectSolidCellContacts(world, occupancy, cells, space);
    PROFILE_END(PROF_SOLID); // PROFILER

    // Pass 3b detects entity pairs sharing at least one broad-phase cell (needs >= 2 entities; with fewer
    // the block count is forced to 0 so the solve below still runs for a lone entity).
    PROFILE_BEGIN(PROF_PAIRS); // PROFILER
    // SNAPSHOT RULE: no pointer into the store is held across a call. The block count is read once and
    // each block's count and ids are copied into locals before the pair loops. DetectCollisionPair
    // can call AddObjectToWorld for collision markers; those are ROLE_EFFECT so they never enter the
    // store today, but the snapshot makes correctness independent of that (pool growth would
    // otherwise invalidate block pointers).
    int n_blocks = (obj_count >= 2) ? CellOccupancy_BlockCount(occupancy) : 0;
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
                    DetectCollisionPair(world, ids[m], ids[n], cell_i,
                                        resolved_collisions, scheduled_world_cmds);
                }
            }
        }
    }

    PROFILE_END(PROF_PAIRS); // PROFILER

    // SOLVE: iterate every cached contact (pairs, container, solid cells) and write the results back. The
    // centre clamp afterwards is the safety net for an axis with no detected wall contact.
    PROFILE_BEGIN(PROF_SOLVE); // PROFILER
    ResolveSleepWakes(world);
    SolveWorldContacts(world, physics_dt);
    ClampEntitiesToContainer(world);
    PROFILE_END(PROF_SOLVE); // PROFILER

    // Recovery runs LAST: it teleports entities by whole cells, which would invalidate contacts already
    // emitted for them. A solid contact still pushes a shallow tunnel out; only an entity whose centre is
    // buried in an absent-cell hole is relocated, one phase later than before (after its contacts).
    PROFILE_BEGIN(PROF_SOLID); // PROFILER
    RecoverEntitiesFromAbsentCells(world, occupancy, cells, space);
    PROFILE_END(PROF_SOLID); // PROFILER
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
// Need assign an area of effect (footprint), i.e. Snapped AABB, for the object based on its radius and update the occupancy of all cells that fall within that area
// otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision.
// Surface2d snapped_aabb = CalculateSnappedAABB(space->basis, object->surface, object->anchor_position);

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