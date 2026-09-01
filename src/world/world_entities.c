/**********************************************************************************************
 *
 * WORLD ENTITY MANAGEMENT
 *
 **********************************************************************************************/

#include "world/world_internal.h"
#include "world/universe.h"

static LArray *GetWorldObjectArrayForArchetype(World2d *world, ArchetypeID array_type)
{
    if (!world)
    {
        return NULL;
    }

    return (array_type == ARCHETYPE_CLOCKED) ? &world->temp_objects : &world->objects;
}

static void FreeEntitySurfaceVectors(Newtonoid2d *entity)
{
    if (!entity)
    {
        return;
    }

    // Entity surfaces are nested allocations, so release them before clearing the entity array.
    ClearLArray(&entity->surface.surface_vectors);
}

// Free every entity surface and the backing allocation of an entity array.
static void FreeWorldEntityArray(LArray *entities)
{
    if (!entities)
    {
        return;
    }

    for (int entity_index = 0; entity_index < entities->count; entity_index++)
    {
        Newtonoid2d *entity = (Newtonoid2d *)LArray_Get(entities, entity_index);
        FreeEntitySurfaceVectors(entity);
    }

    ClearLArray(entities);
}

// Release entity-owned nested surface data and the two entity arrays for a world.
void DestroyWorldEntityStorage(World2d *world)
{
    if (!world)
    {
        return;
    }

    FreeEntitySurfaceVectors(&world->grid_space.object);
    FreeWorldEntityArray(&world->objects);
    FreeWorldEntityArray(&world->temp_objects);
}

// Add an entity after validating its world position and initial spatial cell.
// Registration happens only after those checks so failed additions leave no
// entity or cell state behind.
EntityId AddObjectToWorld(World2d *world, Newtonoid2d *object, EntityId parent_id)
{
    // Object placement is centre-based; grid occupancy still snaps that centre into a cell index.
    Vector2d local_coords = object->anchor_position;
    Space2d *space = &world->grid_space.space;
    int grid_cell_index = GetIndexFromCoords(space, local_coords);
    if (grid_cell_index < 0)
    {
        LOG_WARN("Desired spawn point (%0.2f,%0.2f) out of bounds. Cannot add entity to the world.\n", local_coords.x, local_coords.y);
        return INVALID_ENTITY_ID;
    }

    // Only eligible physical objects are collision-enabled and need room in
    // their initial cell; effects and dead entities remain outside the map.
    int cell_index = -1;
    if (EntityIsEligbleForSpatialMap(object))
    {
        cell_index = grid_cell_index;
        Cell *cells = world->grid_space.space.cells.items;
        Cell *target_cell = &cells[cell_index];
        if (target_cell->occupancy >= MAX_CELL_OCCUPANCY)
        {
            LOG_WARN("Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
            return INVALID_ENTITY_ID;
        }
    }

    object->parent_id = parent_id;

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

    LOG_INFO("CREATED OBJECT (ID %d): Cell %d : Centre(%.1f, %.1f)\n", object->id, cell_index, local_coords.x, local_coords.y);
    return assigned_id;
}

static void EnqueueWorldCommand(LArray *scheduled_events, WorldCmdType type, EntityId object_id, int payload_value,
                                int initial_frame_delay, int interval_frames, int run_limit)
{
    if (!scheduled_events)
    {
        return;
    }

    WorldCommand cmd = {
        .type = type,
        .target_id = object_id,
        .payload_value = payload_value,
        .interval_frames = interval_frames <= 0 ? 1 : interval_frames,
        .run_limit = run_limit,
        .frame_count = 0,
        .initial_frame_delay = initial_frame_delay,
        .active = true};
    LArray_Push(scheduled_events, &cmd);
}

void SetObjectFlag(World2d *world, EntityId object_id, int flag_to_update)
{
    Newtonoid2d *object = GetEntityByID(world, object_id);
    if (!object)
        return;

    object->status_flags = (object->status_flags | flag_to_update);
}

void ClearObjectFlag(World2d *world, EntityId object_id, int flag_to_update)
{
    Newtonoid2d *object = GetEntityByID(world, object_id);
    if (!object)
        return;

    int inverse_flag = ~flag_to_update;
    object->status_flags = object->status_flags & inverse_flag;
}

void UpdateEntityWorldRegistry(FlatMapInt *entity_world_index_registry, EntityId entity_id, int type_flag, int entity_arr_index)
{
    int max_type = (1 << PACKED_INT_HIGH_BITS) - 1;
    int max_index = (1 << PACKED_INT_LOW_BITS) - 1;

    assert(type_flag <= max_type && "Engine Error: type flag exceeds maximum capacity for high bits");
    assert(entity_arr_index <= max_index && "Engine Error: Array index exceeds maximum capacity for low bits.");

    int packed = PACK_INTS(entity_arr_index, type_flag);
    FlatMapInt_InsertOrUpdate(entity_world_index_registry, entity_id, packed);
}

EntityId RegisterEntity(World2d *world, Newtonoid2d *entity)
{
    ArchetypeID array_type;

    if (!(entity->status_flags & FLAG_LIFETIME_CLOCKED))
    {
        array_type = ARCHETYPE_INHABITANT;
    }
    else
    {
        array_type = ARCHETYPE_CLOCKED;
    }

    LArray *world_objects = GetWorldObjectArrayForArchetype(world, array_type);
    if (!world_objects)
    {
        return INVALID_ENTITY_ID;
    }

    if (entity->id > 0)
    {
        // Transfers preserve IDs, so reject an existing ID instead of silently aliasing it.
        if (GetEntityByID(world, entity->id))
        {
            LOG_WARN("Cannot register duplicate entity ID %d in world.\n", entity->id);
            return INVALID_ENTITY_ID;
        }
    }
    else if (world->universe)
    {
        entity->id = Universe_AllocateEntityId(world->universe);
    }
    else
    {
        LOG_ERROR("Cannot register entity without a universe ID allocator.\n");
        return INVALID_ENTITY_ID;
    }

    if (entity->id == INVALID_ENTITY_ID)
    {
        return INVALID_ENTITY_ID;
    }
    LArray_Push(world_objects, entity);
    int assigned_index = world_objects->count - 1;
    UpdateEntityWorldRegistry(&world->entity_world_index_registry, entity->id, array_type, assigned_index);
    LOG_INFO("Registered entity %d at index %d in array type %d", entity->id, assigned_index, array_type);
    return entity->id;
}

static bool RemoveRegisteredEntity(World2d *world, EntityId entity_id, bool free_surface)
{
    int packed_value = 0;
    if (!world || !FlatMapInt_GetValue(&world->entity_world_index_registry, entity_id, &packed_value))
    {
        return false;
    }

    int type = UNPACK_INT_HIGH(packed_value);
    int deleted_idx = UNPACK_INT_LOW(packed_value);
    LArray *world_objects = GetWorldObjectArrayForArchetype(world, (ArchetypeID)type);
    if (!world_objects || deleted_idx < 0 || deleted_idx >= world_objects->count)
    {
        //LOG_WARN("DeregisterEntity: world array empty for entity %d (type=%d). Clearing stale registry entry.\n", entity_id, type);
        FlatMapInt_DeactivateSlot(&world->entity_world_index_registry, entity_id);
        return false;
    }

    Newtonoid2d *entity_to_remove = (Newtonoid2d *)LArray_Get(world_objects, deleted_idx);
    if (free_surface)
    {
        FreeEntitySurfaceVectors(entity_to_remove);
    }

    int last_idx = world_objects->count - 1;
    if (deleted_idx != last_idx)
    {
        // LArray removal uses swap-pop, so the moved last entity needs a new registry index.
        Newtonoid2d *last_entity = (Newtonoid2d *)LArray_Get(world_objects, last_idx);
        if (last_entity)
        {
            UpdateEntityWorldRegistry(&world->entity_world_index_registry, last_entity->id, type, deleted_idx);
        }
        else
        {
            LOG_WARN("DeregisterEntity: last entity lookup failed (last_idx=%zu, count=%d).\n", last_idx, world_objects->count);
        }
    }

    LArray_SwapPopAt(world_objects, deleted_idx);
    FlatMapInt_DeactivateSlot(&world->entity_world_index_registry, entity_id);
    return true;
}

EntityId MoveObjectBetweenWorlds(World2d *source_world, World2d *destination_world,
                                 EntityId object_id, EntityId destination_parent_id,
                                 Vector2d destination_coords)
{
    if (!source_world || !destination_world || source_world == destination_world)
    {
        return INVALID_ENTITY_ID;
    }

    Newtonoid2d *source_entity = (Newtonoid2d *)GetEntityByID(source_world, object_id);
    if (!source_entity)
    {
        LOG_WARN("Cannot move missing entity %d between worlds.\n", object_id);
        return INVALID_ENTITY_ID;
    }

    Vector2d local_coords = destination_coords;
    if (GetIndexFromCoords(&destination_world->grid_space.space, local_coords) < 0)
    {
        LOG_WARN("Cannot move entity %d: position (%0.2f,%0.2f) is outside the destination world.\n",
                 object_id, local_coords.x, local_coords.y);
        return INVALID_ENTITY_ID;
    }

    EntityId original_parent_id = source_entity->parent_id;
    Vector2d original_coords_center = source_entity->anchor_position;
    Vector2d original_coords_origin = source_entity->bounds_origin;
    Newtonoid2d moved_entity = *source_entity;
    moved_entity.anchor_position = destination_coords;
    moved_entity.bounds_origin = (Vector2d){
        destination_coords.x - (moved_entity.bounds_size.x * 0.5f),
        destination_coords.y - (moved_entity.bounds_size.y * 0.5f)};
    if (!RemoveRegisteredEntity(source_world, object_id, false))
    {
        return INVALID_ENTITY_ID;
    }

    EntityId destination_id = AddObjectToWorld(destination_world, &moved_entity, destination_parent_id);
    if (destination_id == INVALID_ENTITY_ID)
    {
        // The source was detached first to prevent both worlds from owning the same entity.
        moved_entity.parent_id = original_parent_id;
        moved_entity.anchor_position = original_coords_center;
        moved_entity.bounds_origin = original_coords_origin;
        if (AddObjectToWorld(source_world, &moved_entity, original_parent_id) == INVALID_ENTITY_ID)
        {
            LOG_ERROR("Failed to restore entity %d after a rejected world transfer.\n", object_id);
        }
        return INVALID_ENTITY_ID;
    }

    RefreshWorldSpatialMap(source_world);
    RefreshWorldSpatialMap(destination_world);
    LOG_INFO("Moved entity %d from world %p to world %p as entity %d.\n",
             object_id, (void *)source_world, (void *)destination_world, destination_id);
    return destination_id;
}

void DeregisterEntity(World2d *world, EntityId entity_id)
{
    if (RemoveRegisteredEntity(world, entity_id, true))
    {
        RefreshWorldSpatialMap(world);
        LOG_INFO("Entity %d safely deregistered.\n", entity_id);
    }
}

void StickEntity(World2d *world, Newtonoid2d *child, Newtonoid2d *parent)
{
    if (!child || !parent)
        return;

    child->parent_id = parent->id;
    child->parent_offset.x = child->anchor_position.x - parent->anchor_position.x;
    child->parent_offset.y = child->anchor_position.y - parent->anchor_position.y;
    child->velocity.x = 0;
    child->velocity.y = 0;
}

void *GetEntityByID(World2d *world, EntityId entity_id)
{
    if (!world || entity_id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    if (world->grid_space.object.id == entity_id)
    {
        // The world root is stored separately from the registered object arrays.
        return &world->grid_space.object;
    }

    int packed_value = 0;
    if (!FlatMapInt_GetValue(&world->entity_world_index_registry, entity_id, &packed_value))
    {
        return NULL;
    }

    int type_flag = UNPACK_INT_HIGH(packed_value);
    int index = UNPACK_INT_LOW(packed_value);

    LArray *world_objects = GetWorldObjectArrayForArchetype(world, (ArchetypeID)type_flag);
    return world_objects ? LArray_Get(world_objects, index) : NULL;
}

void ScheduleEntityFlagSet(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_SET_OBJECT_FLAG,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void ScheduleEntityFlagClear(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_CLEAR_OBJECT_FLAG,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void ScheduleEntityDeletion(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_DELETE_OBJECT,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void RunScheduledWorldCmds(LArray *scheduled_cmds, World2d *world)
{
    WorldCommand *cmds = (WorldCommand *)scheduled_cmds->items;
    size_t i = 0;

    while (i < scheduled_cmds->count)
    {
        if (cmds[i].initial_frame_delay > 0)
        {
            cmds[i].initial_frame_delay--;
            i++;
            continue;
        }

        if (cmds[i].frame_count > 0)
        {
            cmds[i].frame_count--;
            i++;
            continue;
        }

        switch (cmds[i].type)
        {
        case CMD_SET_OBJECT_FLAG:
            SetObjectFlag(world, cmds[i].target_id, cmds[i].payload_value);
            break;
        case CMD_CLEAR_OBJECT_FLAG:
            ClearObjectFlag(world, cmds[i].target_id, cmds[i].payload_value);
            break;
        case CMD_DELETE_OBJECT:
            DeregisterEntity(world, cmds[i].target_id);
            break;
        default:
            LOG_ERROR("Unknown command type %d\n", cmds[i].type);
            break;
        }

        cmds[i].run_count++;
        if (cmds[i].run_count >= cmds[i].run_limit)
        {
            size_t last_idx = scheduled_cmds->count - 1;
            if (i != last_idx)
            {
                cmds[i] = cmds[last_idx];
            }
            scheduled_cmds->count--;
        }
        else
        {
            cmds[i].frame_count = cmds[i].interval_frames;
            i++;
        }
    }
}
