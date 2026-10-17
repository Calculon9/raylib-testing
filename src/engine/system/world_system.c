/**********************************************************************************************
 *
 * WORLD GAMEPLAY SYSTEM
 *
 * Gameplay input, world selection, entity picking, and screen-level world helpers.
 * Simulation and entity storage remain in the world module under src/engine/world.
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
#include "system/command_system.h"
#include "system/debug_overlay_system.h"
#include "system/job_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "input/drag_interaction.h"
#include "combat/projectile.h"
#include "entities/entity_registry.h"
#include "entities/health.h"

// World-level defaults used by the gameplay screen and debug spawning controls.
bool world_grid_debug_labels_enabled = false;
// World grid visibility (a view/display setting, not a debug flag). The debug-overlay facade's
// DEBUG_WORLD_GRID case delegates to this owner.
bool world_grid_overlay_enabled = true;
// Universe-space grid-label visibility (view/display setting). Default OFF, as before. The
// debug-overlay facade's DEBUG_UNIVERSE_GRID_LABELS case delegates here.
bool universe_grid_labels_enabled = false;
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
    Vector2d base_anchor;   // Entity's anchor captured at drag start (multi-move base).
    EntityRoleFlags collision_role_mask;
    bool has_snapshot;
} EntityDragMotionSnapshot;

// Drag-motion snapshots for every entity touched by the active drag. A single-entity drag stores
// one entry; a multi-move stores one per in-world ENTITY member of the SelectionSet. Allocated
// lazily on first use and reused thereafter (count reset, capacity retained) so no per-frame
// allocation occurs during a drag.
static LArray g_drag_motion_snapshots = {0};

// True while a multi-move drag is armed (an already-selected entity with more than one in-world
// ENTITY member was dragged without a modifier). Pinned at drag start, cleared on release and on
// any single-select capture, so the per-frame dispatch decision matches drag-start membership.
static bool g_multi_move_active = false;

// Ensure the snapshot store is allocated and empty, ready to receive this drag's entries.
static void DragMotion_Reset(void)
{
    if (!g_drag_motion_snapshots.items)
    {
        // Lazy one-time allocation; 4 is a sensible starting capacity for a small selection.
        g_drag_motion_snapshots = MakeLArray(4, sizeof(EntityDragMotionSnapshot));
    }
    g_drag_motion_snapshots.count = 0;
}

// Record one entity's motion state into the snapshot store and pause its physics for the drag.
// Saves velocity/acceleration/momentum/collision_role_mask plus the entity's current anchor (the
// multi-move base), then zeroes the entity's collision role so it does not collide while dragged.
static void DragMotion_PushEntity(Newtonoid2d *entity)
{
    if (!entity)
    {
        return;
    }

    EntityDragMotionSnapshot snapshot = {0};
    snapshot.entity = entity;
    snapshot.velocity = entity->velocity;
    snapshot.acceleration = entity->acceleration;
    snapshot.momentum = entity->momentum;
    snapshot.base_anchor = entity->anchor_position;
    snapshot.collision_role_mask = entity->collision_role_mask;
    snapshot.has_snapshot = true;

    entity->collision_role_mask = ENTITY_ROLE_NONE;
    LArray_Push(&g_drag_motion_snapshots, &snapshot);
}

// Forward declarations keep the input router independent of helper ordering.
void CreateAddNewtonoid(int vertice_count, float radius, ShapeBuildType build_type,
                        float mass, ColourRgba colour, Vector2d anchor_position,
                        Vector2d velocity, Vector2d acceleration);
void TogglePause(World2d *world);

// Save motion state for a single dragged entity so manual positioning does not permanently erase
// the simulation state that existed before the drag. Clears the store first, so a single-entity
// drag holds exactly one snapshot (the captured entity). Passing NULL simply empties the store.
static void SnapshotDraggedEntityMotion(Newtonoid2d *entity)
{
    DragMotion_Reset();
    if (!entity)
    {
        return;
    }

    DragMotion_PushEntity(entity);
}

// Restore the saved simulation state for every entity recorded in the snapshot store when a drag
// ends. Each member's velocity/acceleration/momentum/collision_role_mask is restored from its
// stored snapshot, then the store is emptied. Safe to call with an empty store.
static void RestoreDraggedEntitiesMotion(void)
{
    EntityDragMotionSnapshot *snapshot = NULL;
    LArray_ForEach(&g_drag_motion_snapshots, EntityDragMotionSnapshot *, snapshot)
    {
        Newtonoid2d *entity = snapshot->entity;
        if (!entity || !snapshot->has_snapshot)
        {
            continue;
        }

        entity->velocity = snapshot->velocity;
        entity->acceleration = snapshot->acceleration;
        entity->momentum = snapshot->momentum;
        entity->collision_role_mask = snapshot->collision_role_mask;
    }

    g_drag_motion_snapshots.count = 0;
}

// Snapshot every in-world ENTITY member of the SelectionSet for a multi-move drag. The store is
// cleared first, then each ENTITY member is resolved live via Universe_GetEntityByID; a member is
// skipped when it is stale (resolves to NULL) or lives in a different world than the dragged entity
// (cross-world multi-move is out of scope). Every snapshot entry records the member's base anchor
// and pauses its physics, mirroring the single-entity SnapshotDraggedEntityMotion for each member.
static void SnapshotSelectionMotion(int source_world_index)
{
    DragMotion_Reset();

    TargetRef *member = NULL;
    LArray_ForEach(&G_UIState.selection_set, TargetRef *, member)
    {
        // ENTITIES ONLY: WORLD/CELL/NONE members never move on an entity drag.
        if (member->kind != TARGET_KIND_ENTITY)
        {
            continue;
        }

        int member_world_index = -1;
        Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe, member->as.entity,
                                                     &member_world_index);

        // Skip stale ids and out-of-world members (intra-world scope only).
        if (!entity || member_world_index != source_world_index)
        {
            continue;
        }

        DragMotion_PushEntity(entity);
    }
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
    if (G_UIState.entity_create_params)
    {
        Deallocate((void **)&G_UIState.entity_create_params,
                   sizeof(*G_UIState.entity_create_params));
    }
    G_UIState.entity_create_params = AllocateBytes(sizeof(EntityCreateParams));
    if (G_UIState.entity_create_params)
    {
        // Seed sane physics defaults so any consumer (the popup create menu, a
        // programmatic spawn) produces a valid entity even before the lpanel DRAW form
        // has been touched. The struct is otherwise zero-initialised, which would make
        // width/height 0 and fail Entity_ValidateDimensions. The DRAW form's value=
        // attributes overwrite these once that panel loads.
        Newtonoid2dParams *physics = &G_UIState.entity_create_params->physics;
        physics->width = 1.0f;
        physics->height = 1.0f;
        physics->mass = 1.0f;
        physics->restitution = 0.9f;
        physics->friction = 0.5f;
        physics->vertice_count = 4;
    }

    extern void InitCommandSystem(void);
    InitCommandSystem();
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
    // Prune stale members from the central selection set once per frame (purges ids dropped by
    // deletes/moves that went through the existing single paths).
    UIState_ValidateSelectionSet();
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
static InputRouteResult TryHandleFireProjectile(World2d *active_world, InputRouteResult prior_result)
{
    if (!active_world || prior_result != INPUT_ROUTE_IGNORED || !IsKeyPressed(KEY_F))
    {
        return prior_result;
    }

    Newtonoid2d *shooter = UIState_GetSelectedObject();
    int shooter_world_index = -1;
    Universe_GetEntityByID(&G_Universe, shooter ? shooter->id : INVALID_ENTITY_ID, &shooter_world_index);

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
static InputRouteResult TrySelectAndCaptureEntity(World2d *active_world, const InputFrame *input, Vector2d click_world_coords,
                                                  DragInteractionState *game_drag_ctx, InputRouteResult prior_result)
{
    if (!active_world || !input || !game_drag_ctx ||
        prior_result != INPUT_ROUTE_IGNORED || !input->left_down ||
        game_drag_ctx->has_capture)
    {
        return prior_result;
    }

    char log[256] = "";
    int offset = 0;
    AppendLogLine(log, sizeof(log), &offset, "WORLD (%.1f,%.1f) --> ",
                  game_viewport.local_origin.x, game_viewport.local_origin.y);

    Cell *selected_cell = NULL;
    int selected_cell_index = -1;
    Newtonoid2d *closest = ResolveClosestEntityAt(active_world, click_world_coords, &selected_cell,
                                                  &selected_cell_index,log,sizeof(log), &offset);

    // A held Ctrl/Shift turns a click into an additive toggle instead of a replace.
    bool modified = input->ctrl_down || input->shift_down;

    if (!modified)
    {
        // Decide whether this unmodified drag is a multi-move: an already-selected entity with more
        // than one in-world ENTITY member moves the whole selection; anything else is single-move.
        g_multi_move_active = false;
        if (closest)
        {
            int dragged_world_index = -1;
            Universe_GetEntityByID(&G_Universe, closest->id, &dragged_world_index);

            TargetRef dragged_ref = { .kind = TARGET_KIND_ENTITY, .as.entity = closest->id };
            bool dragged_in_set = SelectionSet_Contains(dragged_ref);

            if (dragged_in_set)
            {
                // Count the ENTITY members that live in the dragged entity's world; a multi-move
                // needs at least two (the dragged entity plus one more) to be meaningful.
                int in_world_entity_members = 0;
                TargetRef *member = NULL;
                LArray_ForEach(&G_UIState.selection_set, TargetRef *, member)
                {
                    if (member->kind != TARGET_KIND_ENTITY)
                    {
                        continue;
                    }
                    int member_world_index = -1;
                    if (Universe_GetEntityByID(&G_Universe, member->as.entity,
                                               &member_world_index) &&
                        member_world_index == dragged_world_index)
                    {
                        in_world_entity_members++;
                    }
                }

                if (in_world_entity_members > 1)
                {
                    // Multi-move: keep the set intact (do NOT collapse it via UIState_SetSelection),
                    // keep the dragged entity as the primary single-fields, snapshot every in-world
                    // member and capture on the dragged entity.
                    UIState_SyncPrimaryFieldsFromSet();
                    g_multi_move_active = true;
                    SnapshotSelectionMotion(dragged_world_index);
                    DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_ENTITY, closest, closest->anchor_position);
                    AppendLogLine(log, sizeof(log), &offset,
                                  "--> MULTI-MOVE: ID:%d (%d members)", closest->id,
                                  in_world_entity_members);

                    LOG_INFO("CLICKED (%d,%d) | %s\n",
                             (int)input->pointer_position.x,
                             (int)input->pointer_position.y,
                             log);
                    return INPUT_ROUTE_CAPTURED;
                }
            }
        }

        // Unmodified click (single-move / empty space): today's replace/clear behaviour, unchanged.
        UIState_SetSelection(closest, selected_cell, selected_cell_index);

        if (closest)
        {
            SnapshotDraggedEntityMotion(closest);
            DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_ENTITY, closest, closest->anchor_position);
            AppendLogLine(log, sizeof(log), &offset, "--> SELECTED ENTITY: ID:%d", closest->id);
        }
        else
        {
            SnapshotDraggedEntityMotion(NULL);
            DragInteraction_ClearCapture(game_drag_ctx);
            AppendLogLine(log, sizeof(log), &offset, " --> SELECTED ENTITY: NULL");
        }

        LOG_INFO("CLICKED (%d,%d) | %s\n",
                 (int)input->pointer_position.x,
                 (int)input->pointer_position.y,
                 log);

        return closest ? INPUT_ROUTE_CAPTURED : INPUT_ROUTE_HANDLED;
    }

    // Modified click on empty space: leave the set intact (no clear, no capture).
    // (A cell-only modified pick with no entity is likewise a no-op for the set in Stage 1;
    // cells remain a single-selection concept.)
    if (!closest)
    {
        AppendLogLine(log, sizeof(log), &offset, " --> MODIFIED CLICK (empty): SET UNCHANGED");
        LOG_INFO("CLICKED (%d,%d) | %s\n",
                 (int)input->pointer_position.x,
                 (int)input->pointer_position.y,
                 log);
        return INPUT_ROUTE_HANDLED;
    }

    // Modified click on an entity: toggle its ref into the set and re-sync the primary fields.
    TargetRef ref = { .kind = TARGET_KIND_ENTITY, .as.entity = closest->id };
    bool was_present = SelectionSet_Contains(ref);
    SelectionSet_Toggle(ref);
    UIState_SyncPrimaryFieldsFromSet();

    // A modified-click capture never arms multi-move (unchanged single-entity drag behaviour).
    g_multi_move_active = false;

    if (!was_present)
    {
        // Freshly added: keep it immediately draggable, matching the plain-click feel.
        SnapshotDraggedEntityMotion(closest);
        DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_WORLD_ENTITY, closest, closest->anchor_position);
        AppendLogLine(log, sizeof(log), &offset, "--> TOGGLED IN ENTITY: ID:%d", closest->id);

        LOG_INFO("CLICKED (%d,%d) | %s\n",
                 (int)input->pointer_position.x,
                 (int)input->pointer_position.y,
                 log);
        return INPUT_ROUTE_CAPTURED;
    }

    // Toggled out: drop any capture/snapshot for it.
    SnapshotDraggedEntityMotion(NULL);
    DragInteraction_ClearCapture(game_drag_ctx);
    AppendLogLine(log, sizeof(log), &offset, "--> TOGGLED OUT ENTITY: ID:%d", closest->id);

    LOG_INFO("CLICKED (%d,%d) | %s\n",
             (int)input->pointer_position.x,
             (int)input->pointer_position.y,
             log);
    return INPUT_ROUTE_HANDLED;
}

// Apply one world-space drag delta to a single entity from a fixed base anchor, reusing the
// existing snapped-AABB + clamp + RemapEntityInASpace pipeline. `base_anchor` is the entity's
// anchor captured at drag start; `new_center = base_anchor + drag_delta_world` (clamped to the
// grid). This is the exact per-entity transform lifted out of the former HandleIntraWorldEntityDrag
// body, so both the single and multi-move paths share identical maths.
static void MoveEntityByDragDelta(Newtonoid2d *entity,
                                  World2d *world,
                                  Vector2d base_anchor,
                                  Vector2d drag_delta_world)
{
    Vector2d previous_snapped_aabb_verts[4] = {0};
    CalcSnappedAABB_Vertices(entity->surface.surface_vectors.items,
                             entity->surface.surface_vectors.count,
                             entity->anchor_position,
                             world->grid_space.space.frame.basis,
                             previous_snapped_aabb_verts);

    Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(
        previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);

    Vector2d new_center = VectorSum_2d(base_anchor, drag_delta_world);

    Space2d *space = &world->grid_space.space;
    Vector2d min_bound = space->grid_origin;
    Vector2d max_bound = {
        min_bound.x + (float)space->columns - 0.001f,
        min_bound.y + (float)space->rows - 0.001f};

    new_center.x = ClampFloat(new_center.x, min_bound.x, max_bound.x);
    new_center.y = ClampFloat(new_center.y, min_bound.y, max_bound.y);

    entity->anchor_position = new_center;
    entity->bounds_origin = (Vector2d){
        new_center.x - (entity->bounds_size.x * 0.5f),
        new_center.y - (entity->bounds_size.y * 0.5f)};
    entity->velocity = ZERO_VECTOR_2D;
    entity->acceleration = ZERO_VECTOR_2D;
    entity->momentum = ZERO_VECTOR_2D;

    RemapEntityInASpace(&world->grid_space.space,
                        entity,
                        previous_snapped_aabb_box,
                        &world->entity_space_map);
}

// Compute the world-space drag delta for an intra-world entity drag from the pointer's start/now
// positions resolved into the given world's frame.
static Vector2d ComputeIntraWorldDragDelta(World2d *source_world,
                                           DragInteractionState *game_drag_ctx)
{
    Vector2d initial_world_coords = ResolvePixelToWorldFrame(
        source_world, game_drag_ctx->pointer_state.initial_pos);
    Vector2d current_world_coords = ResolvePixelToWorldFrame(
        source_world, game_drag_ctx->pointer_state.current_pos);
    return VectorDiff_2d(current_world_coords, initial_world_coords);
}

// Move a single dragged entity inside its current world and keep its space map in sync. Thin
// wrapper over MoveEntityByDragDelta using the captured target anchor as the base; behaviour is
// identical to the former inline implementation.
static void HandleIntraWorldEntityDrag(Newtonoid2d *dragged,
                                       World2d *source_world,
                                       DragInteractionState *game_drag_ctx)
{
    Vector2d drag_delta_world = ComputeIntraWorldDragDelta(source_world, game_drag_ctx);
    MoveEntityByDragDelta(dragged, source_world, game_drag_ctx->target_anchor,
                          drag_delta_world);
}

// Apply one world-space drag delta to every selected entity captured at drag start (multi-move).
// The snapshot store (populated by SnapshotSelectionMotion) already holds exactly the in-world
// ENTITY members with their base anchors, so iterating it both supplies each member's base and
// acts as the stale-member guard: the id is re-resolved each frame via Universe_GetEntityByID and
// skipped if it no longer resolves. The dragged entity is one of those members and so is moved
// exactly once here; the single-entity HandleIntraWorldEntityDrag is NOT also called, which is the
// double-move guard.
static void HandleIntraWorldMultiEntityDrag(DragInteractionState *game_drag_ctx,
                                            World2d *source_world,
                                            int source_world_index)
{
    Vector2d drag_delta_world = ComputeIntraWorldDragDelta(source_world, game_drag_ctx);

    EntityDragMotionSnapshot *snapshot = NULL;
    LArray_ForEach(&g_drag_motion_snapshots, EntityDragMotionSnapshot *, snapshot)
    {
        // Re-resolve the id live so a member deleted mid-drag is silently skipped (stale guard).
        int member_world_index = -1;
        Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe, snapshot->entity->id,
                                                     &member_world_index);
        if (!entity || member_world_index != source_world_index)
        {
            continue;
        }

        MoveEntityByDragDelta(entity, source_world, snapshot->base_anchor, drag_delta_world);
    }
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

    // Recover the dragged entity's pre-drag collision role from its snapshot (the single-entity
    // cross-world path holds exactly one snapshot entry: the dragged entity).
    EntityRoleFlags dragged_role_mask = ENTITY_ROLE_NONE;
    EntityDragMotionSnapshot *dragged_snapshot =
        (EntityDragMotionSnapshot *)LArray_Get(&g_drag_motion_snapshots, 0);
    if (dragged_snapshot && dragged_snapshot->has_snapshot)
    {
        dragged_role_mask = dragged_snapshot->collision_role_mask;
    }

    EnqueueMoveEntity(dragged->id,
                      source_world_index,
                      destination_world_index,
                      destination_world->grid_space.object.id,
                      current_world_coords,
                      dragged_role_mask);
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
        // Multi-move applies the same delta to every selected in-world entity; the single path
        // moves only the dragged entity. Both share MoveEntityByDragDelta, so the maths is identical.
        if (g_multi_move_active)
        {
            HandleIntraWorldMultiEntityDrag(game_drag_ctx, source_world, source_world_index);
        }
        else
        {
            HandleIntraWorldEntityDrag(dragged, source_world, game_drag_ctx);
        }
    }
    else
    {
        // DOCUMENTED LIMITATION: cross-world multi-move is out of scope. When the dragged entity
        // crosses into a different world only IT is transferred (existing single-entity path);
        // other selected members (and any member living in another world) are not moved.
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
        // Restore every moved member's physics (single or multi) and clear the multi-move arming.
        RestoreDraggedEntitiesMotion();
        g_multi_move_active = false;
        return INPUT_ROUTE_HANDLED;
    }

    return prior_result;
}

// Commit a marquee box selection into the central Stage 1 SelectionSet. The marquee is captured
// in screen pixels; both corners are converted through the existing camera matrices to the
// relevant world/universe frame (dual-space, mirroring Target_ResolveAtPoint / HandleCapturedEntityDrag).
// `additive` unions with the current selection (Ctrl/Shift held); otherwise the selection is
// replaced (cleared first). The hit-test is a simple linear AABB overlap over existing LArrays —
// the cell-scoped broad-phase optimisation is deliberately deferred (documented limitation).
static void CommitMarqueeSelection(const DragInteractionState *ctx, bool additive)
{
    if (!ctx)
    {
        return;
    }

    // No modifier replaces the whole selection; a held modifier unions with it.
    if (!additive)
    {
        SelectionSet_Clear();
    }

    // Universe-space rectangle: convert both pixel corners through the root world's frame. The
    // camera matrix bakes in rotation/zoom, so AABB2d_FromPoints (min/max over the transformed
    // corners) is used rather than assuming axis-alignment survives the transform.
    Vector2d uni_corners[2] = {
        ResolvePixelToWorldFrame(&G_Universe.root_world, ctx->pointer_state.initial_pos),
        ResolvePixelToWorldFrame(&G_Universe.root_world, ctx->pointer_state.current_pos)};
    Matrix2x2 uni_rect = AABB2d_FromPoints(uni_corners, 2);

    // Root-owned entities are placed in universe space: test them against the universe rect.
    {
        Newtonoid2d *e = NULL;
        LArray_ForEach(&G_Universe.root_world.objects, Newtonoid2d*, e)
        {
            if (AABB2d_Overlaps(uni_rect, AABB2d_FromOriginDimensions(e->bounds_origin, e->bounds_size)))
            {
                SelectionSet_Add((TargetRef){ .kind = TARGET_KIND_ENTITY, .as.entity = e->id });
            }
        }
    }

    // Worlds (universe-space bounds) plus, for each overlapped world, its own entities in that
    // world's local frame. A single pass does the world-bounds test, adds the world ref, and
    // immediately scans that world's entities — no remembered list or temporary allocation.
    int world_count = Universe_GetWorldCount(&G_Universe);
    for (int i = 0; i < world_count; i++)
    {
        World2d *w = Universe_GetWorld(&G_Universe, i);
        if (!w || !w->bounds_valid)
        {
            continue;
        }
        if (!AABB2d_Overlaps(uni_rect, w->bounds))
        {
            continue;
        }

        // The world's cached bounds overlap the box: select the world itself.
        SelectionSet_Add((TargetRef){ .kind = TARGET_KIND_WORLD, .as.world_index = i });

        // Convert the pixel corners into this world's local frame and test its entities there.
        Vector2d loc_corners[2] = {
            ResolvePixelToWorldFrame(w, ctx->pointer_state.initial_pos),
            ResolvePixelToWorldFrame(w, ctx->pointer_state.current_pos)};
        Matrix2x2 loc_rect = AABB2d_FromPoints(loc_corners, 2);

        Newtonoid2d *e = NULL;
        LArray_ForEach(&w->objects, Newtonoid2d*, e)
        {
            if (AABB2d_Overlaps(loc_rect, AABB2d_FromOriginDimensions(e->bounds_origin, e->bounds_size)))
            {
                SelectionSet_Add((TargetRef){ .kind = TARGET_KIND_ENTITY, .as.entity = e->id });
            }
        }
    }

    // Re-sync the back-compat single-valued primary fields from the new set contents.
    UIState_SyncPrimaryFieldsFromSet();
}

// Finish a marquee drag on mouse release: commit the enclosed entities/worlds into the selection
// set (replace when no modifier, union when Ctrl/Shift is held) and clear the drag capture.
// Returns INPUT_ROUTE_HANDLED when it consumed a marquee release; otherwise the prior result.
static InputRouteResult TryHandleMarqueeRelease(const InputFrame *input,
                                                DragInteractionState *game_drag_ctx,
                                                InputRouteResult prior_result)
{
    if (!input || !game_drag_ctx || !input->left_released ||
        !game_drag_ctx->has_capture ||
        game_drag_ctx->target_kind != DRAG_TARGET_MARQUEE)
    {
        return prior_result;
    }

    bool additive = input->ctrl_down || input->shift_down;
    CommitMarqueeSelection(game_drag_ctx, additive);
    DragInteraction_ClearCapture(game_drag_ctx);
    return INPUT_ROUTE_HANDLED;
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

            // Begin a marquee drag only when nothing else captured this frame: the universe stage
            // (earlier) would have claimed any draggable world, and TrySelectAndCaptureEntity only
            // captures on an entity hit, so !has_capture here means the drag began on empty space.
            // The 5px threshold keeps a plain empty-space click behaving exactly as before.
            if (game_drag_ctx && !game_drag_ctx->has_capture &&
                DragInteraction_IsDragActive(game_drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
            {
                DragInteraction_BeginCapture(game_drag_ctx, DRAG_TARGET_MARQUEE, NULL,
                                             game_drag_ctx->pointer_state.initial_pos);
            }
        }
        else
        {
            // Marquee and entity releases are mutually exclusive target kinds; attempt the marquee
            // release first and fall through to the entity release when it did not consume it.
            prior_result = TryHandleMarqueeRelease(input, game_drag_ctx, prior_result);
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
        Newtonoid_ConfigureMetadata(&new_newtonoid, ENTITY_ROLE_NEWTONOID,
                 ENTITY_DEFAULT_COLLISION_ROLE_MASK, ENTITY_CAPABILITY_NONE, ENTITY_CONSTRAINT_NONE,
                         ENTITY_STATUS_FLAG_ALIVE, COLOUR_GAME_INK_RGBA, colour);

        EntityId entity_id = EntityRegistry_AllocateId();
        HealthComponentParams health_params = {.max_health = 3.0f};
        HealthComponent health = {0};
        if (entity_id != INVALID_ENTITY_ID && HealthComponent_Initialise(&health, &health_params) &&
            EntityRegistry_AttachHealth(entity_id, &health))
        {
            new_newtonoid.id = entity_id;
            EntityLifecycle_ApplyAttachedComponent(&new_newtonoid, ENTITY_COMPONENT_HEALTH, &health);
            if (AddObjectToWorld(active_world, &new_newtonoid, active_world->grid_space.object.id) == INVALID_ENTITY_ID)
            {
                EntityRegistry_ReleaseId(entity_id);
            }
        }
        else if (entity_id != INVALID_ENTITY_ID)
        {
            EntityRegistry_ReleaseId(entity_id);
        }
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

// Return the number of live entities in the selected world.
int GetNewtonoidCount(void)
{
    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    return active_world ? active_world->objects.count : 0;
}

// Release the shared job-system resources and entity registry when the gameplay screen unloads.
void UnloadGameplayScreen(void)
{
    ShutdownJobSystem();
    EntityRegistry_Shutdown();
}
